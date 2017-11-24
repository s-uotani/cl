#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <CL/cl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

//ホログラムの画素数
#define X_SIZE 1408
#define Y_SIZE 1080

//円周率
#define PI  3.14159265F
#define PI2 6.28318530F

//光波の波長[]
#define WAVELENGTH 0.633F

//物体点の数
#define POINT_NUMBER 900

//ホログラム面のサンプリング間隔
#define SAMPLING_INTERVAL 10.5F

//物体点の座標を格納する変数
float pls_x[POINT_NUMBER];
float pls_y[POINT_NUMBER];
float pls_z[POINT_NUMBER];

//ホログラム規格化用の定数
#define NORM_CONST 40.5845105F

//エラーコード
#define error_CL(hoge) if(hoge!=0) printf("error code = %d,line = %d\n",hoge,__LINE__);

//デバイス側のホログラム格納領域
unsigned char CPUhologram[X_SIZE*Y_SIZE];
unsigned char GPUhologram[X_SIZE*Y_SIZE];
unsigned char img[X_SIZE*Y_SIZE];

//ビットマップ
#pragma pack(push,1)
typedef struct tagBITMAPFILEHEADER
{
	uint16_t	bfType;
	uint32_t	bfSize;
	uint16_t	bfReserved1;
	uint16_t	bfReserved2;
	uint32_t	bf0ffBits;
}BITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER
{
	uint32_t	biSize;
	int32_t		biWidth;
	int32_t		biHeight;
	uint16_t    biPlanes;
	uint16_t    biBitCount;
	uint32_t	biCompression;
	uint32_t	biSizeImage;
	int32_t		biXPelsPerMeter;
	int32_t		biYPelsPerMeter;
	uint32_t	biCirUsed;
	uint32_t	biCirImportant;
}BITMAPINFOHEADER;

typedef struct tagRGBQUAD
{
	unsigned char  rgbBlue;
	unsigned char  rgbGreen;
	unsigned char  rgbRed;
	unsigned char  rgbReserved;
}RGBQUAD;

//時間計測
double getrusage_sec()
{
	struct rusage t;
	struct timeval tv;
	getrusage(RUSAGE_SELF, &t);
	tv = t.ru_utime;
	return tv.tv_sec + (double)tv.tv_usec*1e-6;
}

int main(){

	int point = POINT_NUMBER;
	size_t mem_size_h = sizeof(unsigned char)*X_SIZE*Y_SIZE;
	size_t mem_size_o = sizeof(float)*POINT_NUMBER;

	int i = 0;
	int j = 0;
	FILE *fp = 0;

	cl_int	status;
	cl_event CPUevent, GPUevent;
	cl_ulong CPUstart, GPUstart;
	cl_ulong CPUend, GPUend;

	//ファイルの読み込み
	float x_data, y_data, z_data;
	fp = fopen("tyranno11646.3d", "rb");
	if (fp == NULL){
		printf("ファイル読み込み失敗\n");
	}
	else{
		fread(&point, sizeof(float), 1, fp);

		for (i = 0; i < POINT_NUMBER; i++){
			fread(&x_data, sizeof(float), 1, fp);
			fread(&y_data, sizeof(float), 1, fp);
			fread(&z_data, sizeof(float), 1, fp);

			pls_x[i] = 40 * ((double)x_data) + 704;
			pls_y[i] = 40 * ((double)y_data) + 540;
			pls_z[i] = PI / (WAVELENGTH * 40 * ((double)z_data) + 100000);

		}
	}
	fclose(fp);

	/*for (i = 0; i < 5; i++){
		printf("%f\n", pls_z[i]);
	}*/


	//プラットフォーム取得
	cl_platform_id	platform_id = NULL;
	status = clGetPlatformIDs(1, &platform_id, NULL);
	error_CL(status);

	char message[1024];
	status = clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE, sizeof(message), message, NULL);
	error_CL(status);
	printf("Platform Profile: %s\n", message);

	status = clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, sizeof(message), message, NULL);
	error_CL(status);
	printf("Platform OpenCL Version: %s\n\n", message);

	//デバイス取得
	//CPU
	cl_device_id	CPUdevice = NULL;
	status = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, 1, &CPUdevice, NULL);
	error_CL(status);
	status = clGetDeviceInfo(CPUdevice, CL_DEVICE_VERSION, sizeof(message), message, NULL);
	error_CL(status);
	printf("CPU OpenCL Version: %s\n", message);
	status = clGetDeviceInfo(CPUdevice, CL_DEVICE_VENDOR, sizeof(message), message, NULL);
	error_CL(status);
	printf("CPU Vendor: %s\n", message);
	status = clGetDeviceInfo(CPUdevice, CL_DEVICE_NAME, sizeof(message), message, NULL);
	error_CL(status);
	printf("CPU Name: %s\n", message);

	//GPU
	cl_device_id	GPUdevice = NULL;
	status = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &GPUdevice, NULL);
	error_CL(status);
	status = clGetDeviceInfo(GPUdevice, CL_DEVICE_VERSION, sizeof(message), message, NULL);
	error_CL(status);
	printf("GPU OpenCL Version: %s\n", message);
	status = clGetDeviceInfo(GPUdevice, CL_DEVICE_VENDOR, sizeof(message), message, NULL);
	error_CL(status);
	printf("GPU Vendor: %s\n", message);
	status = clGetDeviceInfo(GPUdevice, CL_DEVICE_NAME, sizeof(message), message, NULL);
	error_CL(status);
	printf("GPU Name: %s\n\n", message);


	//コンテキスト作成
	//CPU
	cl_context	CPUcontext = NULL;
	CPUcontext = clCreateContext(NULL, 1, &CPUdevice, NULL, NULL, &status);
	error_CL(status);

	//GPU
	cl_context	GPUcontext = NULL;
	GPUcontext = clCreateContext(NULL, 1, &GPUdevice, NULL, NULL, &status);
	error_CL(status);

	//コマンドキュー作成
	//CPU
	cl_command_queue				CPUcommand_queue = NULL;
	cl_command_queue_properties     props[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };
	CPUcommand_queue = clCreateCommandQueueWithProperties(CPUcontext, CPUdevice, props, &status);
	error_CL(status);

	//GPU
	cl_command_queue				GPUcommand_queue = NULL;
	GPUcommand_queue = clCreateCommandQueueWithProperties(GPUcontext, GPUdevice, props, &status);
	error_CL(status);

	//メモリオブジェクト作成
	//CPU
	cl_mem		CPUd_hologram = NULL;
	cl_mem		CPUd_pls_x = NULL;
	cl_mem		CPUd_pls_y = NULL;
	cl_mem		CPUd_pls_z = NULL;
	CPUd_hologram = clCreateBuffer(CPUcontext, CL_MEM_READ_WRITE, mem_size_h, NULL, &status);
	CPUd_pls_x = clCreateBuffer(CPUcontext, CL_MEM_READ_ONLY, mem_size_o, NULL, &status);
	CPUd_pls_y = clCreateBuffer(CPUcontext, CL_MEM_READ_ONLY, mem_size_o, NULL, &status);
	CPUd_pls_z = clCreateBuffer(CPUcontext, CL_MEM_READ_ONLY, mem_size_o, NULL, &status);
	error_CL(status);

	//GPU
	cl_mem		GPUd_hologram = NULL;
	cl_mem		GPUd_pls_x = NULL;
	cl_mem		GPUd_pls_y = NULL;
	cl_mem		GPUd_pls_z = NULL;
	GPUd_hologram = clCreateBuffer(GPUcontext, CL_MEM_READ_WRITE, mem_size_h, NULL, &status);
	GPUd_pls_x = clCreateBuffer(GPUcontext, CL_MEM_READ_ONLY, mem_size_o, NULL, &status);
	GPUd_pls_y = clCreateBuffer(GPUcontext, CL_MEM_READ_ONLY, mem_size_o, NULL, &status);
	GPUd_pls_z = clCreateBuffer(GPUcontext, CL_MEM_READ_ONLY, mem_size_o, NULL, &status);
	error_CL(status);

	//ソースコードの読み込み
	fp = fopen("kernel_source_tyranno.cl", "r");
	if (!fp){
		printf("カーネルの読み込みに失敗");
		exit(1);
	}

	char *source = 0;
	size_t source_size = 0;

	source = (char *)malloc(0x100000);
	source_size = fread(source, 1, 0x100000, fp);
	fclose(fp);

	//プログラムオブジェクト作成
	//CPU
	cl_program		CPUprogram = NULL;
	CPUprogram = clCreateProgramWithSource(CPUcontext, 1, (const char**)&source, (const size_t *)&source_size, &status);
	error_CL(status);

	//GPU
	cl_program		GPUprogram = NULL;
	GPUprogram = clCreateProgramWithSource(GPUcontext, 1, (const char**)&source, (const size_t *)&source_size, &status);
	error_CL(status);

	//カーネル関数ビルド
	//CPU
	status = clBuildProgram(CPUprogram, 1, &CPUdevice, NULL, NULL, NULL);
	error_CL(status);

	//CPU
	status = clBuildProgram(GPUprogram, 1, &GPUdevice, NULL, NULL, NULL);
	error_CL(status);

	//カーネルオブジェクトの作成
	//CPU
	cl_kernel		CPUkernel = NULL;
	CPUkernel = clCreateKernel(CPUprogram, "kernel_source_tyranno", &status);
	error_CL(status);

	//GPU
	cl_kernel		GPUkernel = NULL;
	GPUkernel = clCreateKernel(GPUprogram, "kernel_source_tyranno", &status);
	error_CL(status);

	//デバイスメモリへのデータ転送
	//CPU
	status = clEnqueueWriteBuffer(CPUcommand_queue, CPUd_pls_x, CL_TRUE, 0, mem_size_o, pls_x, 0, NULL, NULL);
	status = clEnqueueWriteBuffer(CPUcommand_queue, CPUd_pls_y, CL_TRUE, 0, mem_size_o, pls_y, 0, NULL, NULL);
	status = clEnqueueWriteBuffer(CPUcommand_queue, CPUd_pls_z, CL_TRUE, 0, mem_size_o, pls_z, 0, NULL, NULL);
	error_CL(status);

	//GPU
	status = clEnqueueWriteBuffer(GPUcommand_queue, GPUd_pls_x, CL_TRUE, 0, mem_size_o, pls_x, 0, NULL, NULL);
	status = clEnqueueWriteBuffer(GPUcommand_queue, GPUd_pls_y, CL_TRUE, 0, mem_size_o, pls_y, 0, NULL, NULL);
	status = clEnqueueWriteBuffer(GPUcommand_queue, GPUd_pls_z, CL_TRUE, 0, mem_size_o, pls_z, 0, NULL, NULL);
	error_CL(status);

	//カーネル関数の引数設定
	//CPU
	status = clSetKernelArg(CPUkernel, 0, sizeof(cl_mem), (void *)&CPUd_pls_x);
	status = clSetKernelArg(CPUkernel, 1, sizeof(cl_mem), (void *)&CPUd_pls_y);
	status = clSetKernelArg(CPUkernel, 2, sizeof(cl_mem), (void *)&CPUd_pls_z);
	status = clSetKernelArg(CPUkernel, 3, sizeof(cl_mem), (void *)&CPUd_hologram);
	error_CL(status);

	//GPU
	status = clSetKernelArg(GPUkernel, 0, sizeof(cl_mem), (void *)&GPUd_pls_x);
	status = clSetKernelArg(GPUkernel, 1, sizeof(cl_mem), (void *)&GPUd_pls_y);
	status = clSetKernelArg(GPUkernel, 2, sizeof(cl_mem), (void *)&GPUd_pls_z);
	status = clSetKernelArg(GPUkernel, 3, sizeof(cl_mem), (void *)&GPUd_hologram);
	error_CL(status);

	//カーネル関数実行
	size_t global_size[2];
	size_t local_size[2];

	global_size[0] = X_SIZE;
	global_size[1] = Y_SIZE;
	local_size[0] = 128;
	local_size[1] = 2;

	double start1, end1;
	double start2, end2;

	start1 = getrusage_sec();	//CPU,GPU計測開始

	//CPU
	status = clEnqueueNDRangeKernel(CPUcommand_queue, CPUkernel, 2, NULL, global_size, local_size, 0, NULL, &CPUevent);
	error_CL(status);

	end1 = getrusage_sec();

	start2 = getrusage_sec();

	//GPU
	status = clEnqueueNDRangeKernel(GPUcommand_queue, GPUkernel, 2, NULL, global_size, local_size, 0, NULL, &GPUevent);
	error_CL(status);

	end2 = getrusage_sec();

	//時間計測
	status = clWaitForEvents(1, &CPUevent);
	error_CL(status);
	status = clGetEventProfilingInfo(CPUevent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &CPUstart, NULL);
	error_CL(status);
	status = clGetEventProfilingInfo(CPUevent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &CPUend, NULL);
	error_CL(status);

	double CPUkernel_time;
	CPUkernel_time = CPUend - CPUstart;

	//時間計測
	status = clWaitForEvents(1, &GPUevent);
	error_CL(status);
	status = clGetEventProfilingInfo(GPUevent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &GPUstart, NULL);
	error_CL(status);
	status = clGetEventProfilingInfo(GPUevent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &GPUend, NULL);
	error_CL(status);

	double GPUkernel_time;
	GPUkernel_time = GPUend - GPUstart;

	printf("CPU Kernel Time(tyranno): %f [ms]\n", (CPUkernel_time / 1000000.0));
	printf("GPU Kernel Time(tyranno): %f [ms]\n\n", (GPUkernel_time / 1000000.0));

	printf("CPU,GPU Kernel Time(getrusage): %lf [ms]\n", (end1 - start1 + end2 - start2) * 1000);

	printf("CPU Kernel Time(getrusage): %lf [ms]\n", (end2 - start1) * 1000);
	printf("Time(getrusage): %lf [ms]\n\n", (start2 - end1) * 1000);

	//デバイスからホストへのデータ転送
	//CPU
	status = clEnqueueReadBuffer(CPUcommand_queue, CPUd_hologram, CL_TRUE, 0, mem_size_h, CPUhologram, 0, NULL, NULL);
	error_CL(status);

	//GPU
	status = clEnqueueReadBuffer(GPUcommand_queue, GPUd_hologram, CL_TRUE, 0, mem_size_h, GPUhologram, 0, NULL, NULL);
	error_CL(status);

	/*for (i = 0; i < 10; i++){
		printf("%u\n", CPUhologram[i]);
	}
	printf("\n");
	for (i = 0; i < 10; i++){
		printf("%u\n", GPUhologram[i]);
	}*/

	//ビットマップ用配列
	BITMAPFILEHEADER    BmpFileHeader;
	BITMAPINFOHEADER    BmpInfoHeader;
	RGBQUAD             RGBQuad[256];

	BmpFileHeader.bfType = 19778;
	BmpFileHeader.bfSize = 14 + 40 + 1024 + (X_SIZE * Y_SIZE);
	BmpFileHeader.bfReserved1 = 0;
	BmpFileHeader.bfReserved2 = 0;
	BmpFileHeader.bf0ffBits = 14 + 40 + 1024;

	BmpInfoHeader.biSize = 40;
	BmpInfoHeader.biWidth = X_SIZE;
	BmpInfoHeader.biHeight = Y_SIZE;
	BmpInfoHeader.biPlanes = 1;
	BmpInfoHeader.biBitCount = 8;     //256階調
	BmpInfoHeader.biCompression = 0L;
	BmpInfoHeader.biSizeImage = 0L;
	BmpInfoHeader.biXPelsPerMeter = 0L;
	BmpInfoHeader.biYPelsPerMeter = 0L;
	BmpInfoHeader.biCirUsed = 0L;
	BmpInfoHeader.biCirImportant = 0L;

	for (i = 0; i<256; i++){
		RGBQuad[i].rgbBlue = i;
		RGBQuad[i].rgbGreen = i;
		RGBQuad[i].rgbRed = i;
		RGBQuad[i].rgbReserved = 0;
	}

	//最大値・最小値・中間値
	//CPU
	unsigned char CPUmin_tmp, CPUmax_tmp, CPUmid_tmp;

	CPUmin_tmp = CPUhologram[0];
	CPUmax_tmp = CPUhologram[0];

	for (j = 0; j < Y_SIZE; j++){
		for (i = 0; i < X_SIZE; i++){
			if (CPUmin_tmp > CPUhologram[j*X_SIZE + i]){
				CPUmin_tmp = CPUhologram[j*X_SIZE + i];
			}
			if (CPUmax_tmp < CPUhologram[j*X_SIZE + i]){
				CPUmax_tmp = CPUhologram[j*X_SIZE + i];
			}
		}
	}

	CPUmid_tmp = (CPUmin_tmp + CPUmax_tmp)*0.5;

	//GPU
	unsigned char GPUmin_tmp, GPUmax_tmp, GPUmid_tmp;

	GPUmin_tmp = GPUhologram[0];
	GPUmax_tmp = GPUhologram[0];

	for (j = 0; j < Y_SIZE; j++){
		for (i = 0; i < X_SIZE; i++){
			if (GPUmin_tmp > GPUhologram[j*X_SIZE + i]){
				GPUmin_tmp = GPUhologram[j*X_SIZE + i];
			}
			if (GPUmax_tmp < GPUhologram[j*X_SIZE + i]){
				GPUmax_tmp = GPUhologram[j*X_SIZE + i];
			}
		}
	}

	GPUmid_tmp = (GPUmin_tmp + GPUmax_tmp)*0.5;

	//条件分岐
	//CPU
	for (i = 0; i < X_SIZE*Y_SIZE; i++){
		if (CPUmid_tmp>CPUhologram[i]){
			CPUhologram[i] = 0;
		}
		else{
			CPUhologram[i] = 255;
		}
	}

	//GPU
	for (i = 0; i < X_SIZE*Y_SIZE; i++){
		if (GPUmid_tmp>GPUhologram[i]){
			GPUhologram[i] = 0;
		}
		else{
			GPUhologram[i] = 255;
		}
	}
	

	//ファイルに書き込み
	//CPU	
	fp = fopen("amd_CPU_tyranno.bmp", "wb");

	fwrite(&BmpFileHeader.bfType, sizeof(BmpFileHeader.bfType), 1, fp);
	fwrite(&BmpFileHeader.bfSize, sizeof(BmpFileHeader.bfSize), 1, fp);
	fwrite(&BmpFileHeader.bfReserved1, sizeof(BmpFileHeader.bfReserved1), 1, fp);
	fwrite(&BmpFileHeader.bfReserved2, sizeof(BmpFileHeader.bfReserved2), 1, fp);
	fwrite(&BmpFileHeader.bf0ffBits, sizeof(BmpFileHeader.bf0ffBits), 1, fp);
	fwrite(&BmpInfoHeader, sizeof(BmpInfoHeader), 1, fp);
	fwrite(&RGBQuad[0], sizeof(RGBQuad[0]), 256, fp);

	fwrite(&CPUhologram, sizeof(CPUhologram[0]), X_SIZE*Y_SIZE, fp);

	fclose(fp);

	//GPU	
	fp = fopen("amd_GPU_tyranno.bmp", "wb");

	fwrite(&BmpFileHeader.bfType, sizeof(BmpFileHeader.bfType), 1, fp);
	fwrite(&BmpFileHeader.bfSize, sizeof(BmpFileHeader.bfSize), 1, fp);
	fwrite(&BmpFileHeader.bfReserved1, sizeof(BmpFileHeader.bfReserved1), 1, fp);
	fwrite(&BmpFileHeader.bfReserved2, sizeof(BmpFileHeader.bfReserved2), 1, fp);
	fwrite(&BmpFileHeader.bf0ffBits, sizeof(BmpFileHeader.bf0ffBits), 1, fp);
	fwrite(&BmpInfoHeader, sizeof(BmpInfoHeader), 1, fp);
	fwrite(&RGBQuad[0], sizeof(RGBQuad[0]), 256, fp);

	fwrite(&GPUhologram, sizeof(GPUhologram[0]), X_SIZE*Y_SIZE, fp);

	fclose(fp);


	//終了
	clReleaseDevice(CPUdevice);
	clReleaseDevice(GPUdevice);
	clReleaseContext(CPUcontext);
	clReleaseContext(GPUcontext);
	clFinish(CPUcommand_queue);
	clFinish(GPUcommand_queue);
	clReleaseCommandQueue(CPUcommand_queue);
	clReleaseCommandQueue(GPUcommand_queue);
	clReleaseMemObject(CPUd_pls_x);
	clReleaseMemObject(CPUd_pls_y);
	clReleaseMemObject(CPUd_pls_z);
	clReleaseMemObject(CPUd_hologram);
	clReleaseMemObject(GPUd_pls_x);
	clReleaseMemObject(GPUd_pls_y);
	clReleaseMemObject(GPUd_pls_z);
	clReleaseMemObject(GPUd_hologram);
	clReleaseKernel(CPUkernel);
	clReleaseKernel(GPUkernel);


	return 0;

}
