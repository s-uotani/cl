#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <CL/cl.h>

//ホログラムの画素数
#define X_SIZE 1408
#define Y_SIZE 1080

//円周率
#define PI  3.14159265F
#define PI2 6.28318530F

//光波の波長[]
#define WAVELENGTH 0.633F

//物体点の数
#define POINT_NUMBER 284

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
unsigned char hologram[X_SIZE*Y_SIZE];
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

int main(){

	int point = POINT_NUMBER;
	size_t mem_size_h = sizeof(unsigned char)*X_SIZE*Y_SIZE;
	size_t mem_size_o = sizeof(float)*POINT_NUMBER;

	int i = 0;
	int j = 0;
	FILE *fp = 0;

	cl_int	status;
	cl_event event_kernel;
	cl_ulong start;
	cl_ulong end;

	//ファイルの読み込み
	int x_data, y_data, z_data;

	fp = fopen("cube284.3d", "rb");
	if (fp == NULL){
		printf("ファイル読み込み失敗\n");
	}
	else{
		fread(&point, sizeof(int), 1, fp);

		for (i = 0; i < POINT_NUMBER; i++){
			fread(&x_data, sizeof(int), 1, fp);
			fread(&y_data, sizeof(int), 1, fp);
			fread(&z_data, sizeof(int), 1, fp);

			pls_x[i] = 40 * ((float)x_data) + 704;
			pls_y[i] = 40 * ((float)y_data) + 540;
			pls_z[i] = PI / (WAVELENGTH * 40 * ((float)z_data) + 10000);

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
	cl_device_id	device_id = NULL;
	status = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, 1, &device_id, NULL);
	error_CL(status);

	status = clGetDeviceInfo(device_id, CL_DEVICE_VERSION, sizeof(message), message, NULL);
	error_CL(status);
	printf("Device OpenCL Version: %s\n", message);

	status = clGetDeviceInfo(device_id, CL_DEVICE_VENDOR, sizeof(message), message, NULL);
	error_CL(status);
	printf("CPU Vendor: %s\n", message);

	status = clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(message), message, NULL);
	error_CL(status);
	printf("CPU Name: %s\n", message);

	status = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
	error_CL(status);

	status = clGetDeviceInfo(device_id, CL_DEVICE_VENDOR, sizeof(message), message, NULL);
	error_CL(status);
	printf("GPU Vendor: %s\n", message);

	status = clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(message), message, NULL);
	error_CL(status);
	printf("GPU Name: %s\n\n", message);

	//コンテキスト作成
	cl_context	context = NULL;
	context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &status);
	error_CL(status);

	//コマンドキュー作成
	cl_command_queue				command_queue = NULL;
	command_queue = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &status);
	error_CL(status);

	//メモリオブジェクト作成
	cl_mem		d_hologram = NULL;
	cl_mem		d_pls_x = NULL;
	cl_mem		d_pls_y = NULL;
	cl_mem		d_pls_z = NULL;
	d_hologram = clCreateBuffer(context, CL_MEM_READ_WRITE, mem_size_h, NULL, &status);
	d_pls_x = clCreateBuffer(context, CL_MEM_READ_ONLY, mem_size_o, NULL, &status);
	d_pls_y = clCreateBuffer(context, CL_MEM_READ_ONLY, mem_size_o, NULL, &status);
	d_pls_z = clCreateBuffer(context, CL_MEM_READ_ONLY, mem_size_o, NULL, &status);
	error_CL(status);

	//ソースコードの読み込み
	fp = fopen("kernel_source_cube.cl", "r");
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
	cl_program		program = NULL;
	program = clCreateProgramWithSource(context, 1, (const char**)&source, (const size_t *)&source_size, &status);
	error_CL(status);

	//カーネル関数ビルド
	status = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
	error_CL(status);

	/*
	size_t log;
	cl_build_status bldstatus;
	status = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_STATUS, sizeof(bldstatus), &bldstatus, &log);
	error_CL(status);

	if (bldstatus == CL_BUILD_SUCCESS) printf("Build Status: CL_BUILD_SUCCESS\n");
	if (bldstatus == CL_BUILD_NONE) printf("Build Status: CL_BUILD_NONE\n");
	if (bldstatus == CL_BUILD_ERROR) printf("Build Status: CL_BUILD_ERROR\n");
	if (bldstatus == CL_BUILD_IN_PROGRESS) printf("Build Status: CL_BUILD_IN_PROGRESS\n");
	*/

	//カーネルオブジェクトの作成
	cl_kernel		kernel = NULL;
	kernel = clCreateKernel(program, "kernel_source_cube", &status);
	error_CL(status);

	//デバイスメモリへのデータ転送
	status = clEnqueueWriteBuffer(command_queue, d_pls_x, CL_TRUE, 0, mem_size_o, pls_x, 0, NULL, NULL);
	status = clEnqueueWriteBuffer(command_queue, d_pls_y, CL_TRUE, 0, mem_size_o, pls_y, 0, NULL, NULL);
	status = clEnqueueWriteBuffer(command_queue, d_pls_z, CL_TRUE, 0, mem_size_o, pls_z, 0, NULL, NULL);
	error_CL(status);

	//カーネル関数の引数設定
	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&d_pls_x);
	status = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&d_pls_y);
	status = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&d_pls_z);
	status = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void *)&d_hologram);
	error_CL(status);

	//カーネル関数実行
	size_t global_size[2];
	size_t local_size[2];

	global_size[0] = X_SIZE;
	global_size[1] = Y_SIZE;
	local_size[0] = 128;
	local_size[1] = 2;

	status = clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, global_size, local_size, 0, NULL, &event_kernel);
	error_CL(status);

	//時間計測
	status = clWaitForEvents(1, &event_kernel);
	error_CL(status);
	status = clGetEventProfilingInfo(event_kernel, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
	error_CL(status);
	status = clGetEventProfilingInfo(event_kernel, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
	error_CL(status);

	double kernel_time;
	kernel_time = end - start;

	printf("Kernel Time(cube1.2): %f [ms]\n\n", (kernel_time / 1000000.0));

	//デバイスからホストへのデータ転送
	status = clEnqueueReadBuffer(command_queue, d_hologram, CL_TRUE, 0, mem_size_h, hologram, 0, NULL, NULL);
	error_CL(status);

	/*for (i = 0; i < 10; i++){
	printf("%u\n", hologram[i]);
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
	unsigned char min_tmp, max_tmp, mid_tmp;

	min_tmp = hologram[0];
	max_tmp = hologram[0];

	for (j = 0; j < Y_SIZE; j++){
		for (i = 0; i < X_SIZE; i++){
			if (min_tmp > hologram[j*X_SIZE + i]){
				min_tmp = hologram[j*X_SIZE + i];
			}
			if (max_tmp < hologram[j*X_SIZE + i]){
				max_tmp = hologram[j*X_SIZE + i];
			}
		}
	}

	mid_tmp = (min_tmp + max_tmp)*0.5;

	//条件分岐
	for (i = 0; i < X_SIZE*Y_SIZE; i++){
		if (mid_tmp>hologram[i]){
			hologram[i] = 0;
		}
		else{
			hologram[i] = 255;
		}
	}

	//ファイルに書き込み
	fp = fopen("amd1.2_cube.bmp", "wb");

	fwrite(&BmpFileHeader.bfType, sizeof(BmpFileHeader.bfType), 1, fp);
	fwrite(&BmpFileHeader.bfSize, sizeof(BmpFileHeader.bfSize), 1, fp);
	fwrite(&BmpFileHeader.bfReserved1, sizeof(BmpFileHeader.bfReserved1), 1, fp);
	fwrite(&BmpFileHeader.bfReserved2, sizeof(BmpFileHeader.bfReserved2), 1, fp);
	fwrite(&BmpFileHeader.bf0ffBits, sizeof(BmpFileHeader.bf0ffBits), 1, fp);
	fwrite(&BmpInfoHeader, sizeof(BmpInfoHeader), 1, fp);
	fwrite(&RGBQuad[0], sizeof(RGBQuad[0]), 256, fp);

	fwrite(&hologram, sizeof(hologram[0]), X_SIZE*Y_SIZE, fp);

	fclose(fp);

	//終了
	clFinish(command_queue);
	clReleaseCommandQueue(command_queue);
	clReleaseContext(context);
	clReleaseMemObject(d_pls_x);
	clReleaseMemObject(d_pls_y);
	clReleaseMemObject(d_pls_z);
	clReleaseMemObject(d_hologram);
	clReleaseKernel(kernel);
	clReleaseDevice(device_id);

	return 0;

}
