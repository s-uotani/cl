#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#define MEM_SIZE (128)
#define MAX_SOURCE_SIZE (0x100000)

int main() {
	cl_device_id device_id = NULL;
	cl_context context = NULL;
	cl_command_queue command_queue = NULL;
	cl_mem memobj = NULL;
	cl_program program = NULL;
	cl_kernel kernel = NULL;
	cl_platform_id platform_id = NULL;
	cl_uint ret_num_devices;
	cl_uint ret_num_platforms;
	cl_int ret;

	char string[MEM_SIZE];

	FILE *fp;
	char filename[] = "./hello.cl";
	char *source_str;
	size_t source_size;

	/*--------------------カーネルを含むソースのロード--------------------*/
	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Failed to load kernel\n");
		exit(1);
	}
	source_str = (char*)malloc(MAX_SOURCE_SIZE);
	source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
	fclose(fp);

	/*--------------------プラットフォーム・デバイス情報取得--------------------*/
	ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
	ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);

	/*--------------------OpenCLコンテキスト作成--------------------*/
	context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);

	/*--------------------コマンドキュー作成--------------------*/
	command_queue = clCreateCommandQueue(context, device_id, 0, &ret);

	/*--------------------メモリバッファ作成--------------------*/
	memobj = clCreateBuffer(context, CL_MEM_READ_WRITE, MEM_SIZE * sizeof(char), NULL, &ret);

	/*--------------------読み込んだソースからカーネルプログラム作成--------------------*/
	program =clCreateProgramWithSource(context, 1, (const char **)&source_str, (const size_t *)&source_size, &ret);

	/*--------------------カーネルプログラムをビルド--------------------*/
	ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);

	/*--------------------OpenCLカーネル作成--------------------*/
	kernel = clCreateKernel(program, "hello", &ret);

	/*--------------------OpenCLカーネル引数の設定--------------------*/
	ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&memobj);

	/*--------------------OpenCLカーネル実行--------------------*/
	ret = clEnqueueTask(command_queue, kernel, 0, NULL, NULL);

	/*--------------------メモリバッファから結果取得--------------------*/
	ret = clEnqueueReadBuffer(command_queue, memobj, CL_TRUE, 0, MEM_SIZE * sizeof(char), string, 0, NULL, NULL);

	/*--------------------結果表示--------------------*/
	puts(string);

	/*--------------------終了処理--------------------*/
	ret = clFlush(command_queue);
	ret = clFinish(command_queue);
	ret = clReleaseKernel(kernel);
	ret = clReleaseProgram(program);
	ret = clReleaseMemObject(memobj);
	ret = clReleaseCommandQueue(command_queue);
	ret = clReleaseContext(context);

	free(source_str);

	return 0;
}
