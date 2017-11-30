#include <stdio.h>
#include <stdint.h>		//uint32_tは符号なしintで4バイトに指定
#include <stdlib.h> 	//記憶域管理を使うため
#include <sys/time.h>
#include <CL/cl.h>

#define width 1024
#define heigth 1024
#define pixel width*heigth
#define POINT_NUMBER 284
#define PI 3.14159265F
#define WAVELENGTH 0.633F

//画像生成用の配列
unsigned char hologram[pixel];
//物体点座標用配列
float pls_x[POINT_NUMBER];
float pls_y[POINT_NUMBER];
float pls_z[POINT_NUMBER];


/*--------------------BMPの構造体--------------------*/
#pragma pack(push,1)
typedef struct tagBITMAPFILEHEADER {	//構造体BITMAPFILEHEADERはファイルの先頭に来るもので，サイズは14 byte
	unsigned short	bfType;				//bfTypeは，bmp形式であることを示すため，"BM"が入る
	uint32_t 		bfSize;				//bfsizeは，ファイル全体のバイト数
	unsigned short	bfReserved1;		//bfReserved1と2は予約領域で，0になる
	unsigned short	bfReserved2;
	uint32_t		bf0ffBits;			//bf0ffBitsは先頭から画素データまでのバイト数
}
BITMAPFILEHEADER;

#pragma pack(pop)
typedef struct tagBITMAPINFOHEADER {	//BITMAPINFOHEADERはbmpファイルの画像の情報の構造体で，サイズは40 byte
	uint32_t		biSize;				//画像のサイズ
	int32_t			biWidth;			//横の画素数
	int32_t			biHeight;			//縦の画素数
	unsigned short	biPlanes;			//1
	unsigned short	biBitCount;			//一画素あたりの色の数のbit数．今回は8
	uint32_t		biCompression;		//圧縮タイプを表す．bmpは非圧縮なので0
	uint32_t		biSizeImage;		//bmp配列のサイズを表す．biCompression=0なら基本的に0
	int32_t			biXPelsPerMeter;	//biXPelsPerMeterとbiYPelsPerMeterは基本的に0
	int32_t			biYPelsPerMeter;
	uint32_t		biCirUsed;			//0
	uint32_t		biCirImportant;		//0
}
BITMAPINFOHEADER;

typedef struct tagRGBQUAD {
	unsigned char	rgbBlue;
	unsigned char	rgbGreen;
	unsigned char	rgbRed;
	unsigned char	rgbReserved;
}
RGBQUAD;
/*--------------------------------------------------*/


/*--------------------main関数--------------------*/
int main() {
	//ホスト側の変数
	int i;
	int points;
	int x_buf, y_buf, z_buf;		//データを一時的に溜めておくための変数
	unsigned char min, max, mid;	//2値化に用いる
	FILE *fp;

	//3Dファイルの読み込み
	fp = fopen("cube284.3d","rb");		//バイナリで読み込み
	if (!fp) {
		printf("3D file not found!\n");
		exit(1);
	}
	fread(&points, sizeof(int), 1, fp);	//データのアドレス，サイズ，個数，ファイルポインタを指定
	printf("the number of points is %d\n", points);
	/*
	//取り出した物体点を入れる配列
	int pls_x[points];			//~~データを読み込むことで初めてこの配列が定義できる~~
	int pls_y[points];
	float pls_z[points];
	int x_buf, y_buf, z_buf;	//データを一時的に溜めておくための変数
	 */
	//各バッファに物体点座標を取り込み，ホログラム面と物体点の位置を考慮したデータを各配列に入れる
	for (i=0; i<points; i++) {
		fread(&x_buf, sizeof(int), 1, fp);
		fread(&y_buf, sizeof(int), 1, fp);
		fread(&z_buf, sizeof(int), 1, fp);

		pls_x[i] = (float)x_buf*40+width/2;	//物体点を離すために物体点座標に40を掛け，中心の座標を足す
		pls_y[i] = (float)y_buf*40+heigth/2;
		pls_z[i] = PI/((float)z_buf*40*WAVELENGTH+10000.0F);
	}
	fclose(fp);


	size_t mem_size_h = sizeof(unsigned char)*pixel;
	size_t mem_size_o = sizeof(float)*points;

	/*--------------------OpenCL変数宣言--------------------*/
	//OpenCL変数
	cl_device_id		device_id		= NULL;
	cl_context			context			= NULL;
	cl_command_queue	command_queue	= NULL;
	cl_program			program			= NULL;
	cl_kernel			kernel			= NULL;
	cl_platform_id		platform_id		= NULL;
	//時間計測用変数
	cl_int		status;
	cl_event	event;
	cl_ulong	start;
	cl_ulong	end;

	//カーネル実行用変数
	size_t	global_size[2];
	size_t	local_size[2];
	char	*source	= 0;
	size_t	source_size	= 0;

	//メモリオブジェクト（デバイス側で結果を格納する変数）
	cl_mem	d_pls_x		= NULL;
	cl_mem	d_pls_y		= NULL;
	cl_mem	d_pls_z		= NULL;
	cl_mem	d_hologram	= NULL;

	//カーネルオブジェクト
	cl_kernel	holo_calculation	= NULL;
	/*--------------------------------------------------*/


	/*--------------------OpenCLの初期化--------------------*/
	//プラットフォームの取得
	//プラットフォームはそれぞれ固有のIDを持つため、使用できるプラットフォームのIDを取得する関数
	clGetPlatformIDs(
		1,				//プラットフォーム数
		&platform_id,	//プラットフォームIDを格納する変数（複数なら配列）
		NULL			//利用できるプラットフォーム数の戻り地
	);

	//デバイスの取得
	//デバイスも固有のIDを持つため、使用できるデバイスのIDを取得する関数
	clGetDeviceIDs(
		platform_id,			//clGetPlatformIDs関数で取得したプラットフォームID
		CL_DEVICE_TYPE_GPU,		//利用したいデバイスの種類
		1,						//利用するデバイス数
		&device_id,				//cl_device_id型のハンドル(ポインタ)
		NULL					//第2引数で指定したデバイス数の戻り地
	);

	//コンテキストの作成
	//いくつかのデバイスやメモリをまとめて管理するためにコンテキスト環境を作成する関数
	context = clCreateContext(
		NULL,		//コンテキストプロパティの指定
		1,			//第3引数のデバイス数
		&device_id,	//clGetDeviceIDs関数で取得されたデバイス
		NULL,		//コールバック関数
		NULL,		//コールバック関数が呼び出された時にデータが渡されるポインタ
		NULL		//エラーコード
	);

	//コマンドキューの作成
	//デバイスとホストをつなげるコマンドキューを作成する関数
	command_queue = clCreateCommandQueue(
		context,					//OpenCLコンテキスト
		device_id,					//第１引数に関連づけられたデバイス(コマンドを実行するデバイス)
		CL_QUEUE_PROFILING_ENABLE,	//コマンドキューに適用するプロパティのリスト
		NULL						//エラーコードを格納する変数
	);

	//ソースコードの読み込み
	//OpenCLではカーネルソースに記載されてるカーネル関数群をプログラムオブジェクトという単位で管理する
	fp = fopen("phase_OpenCL.cl", "r");
	if (!fp) {
		printf("kernel file not found!\n");
		exit(1);
	}
	source = (char *)malloc(0x100000);
	source_size = fread(source, 1, 0x100000, fp);
	fclose(fp);

	//プログラムオブジェクトの作成
	//読み込んだカーネルソースからプログラムオブジェクトを作成する関数
	program = clCreateProgramWithSource(
		context,						//正常なOpenCLコンテキストを引き渡す(必ずやる)
		1,								//第３引数のデバイス数
		(const char**)&source,			//カーネルプログラムのソース
		(const size_t *)&source_size,	//ソースの文字列のサイズを指定
		NULL							//エラーコードを格納する変数
	);

	//カーネル関数のビルド
	//作成したプログラムオブジェクトからカーネル関数をビルドする
	clBuildProgram(
		program,	//ビルド対象のプログラムオブジェクト
		1,			//第3引数で与えたデバイス数
		&device_id,	//プログラムオブジェクトに関連付けられたデバイスのリストへのポインタを指定
		NULL,		//コンパイラに渡す引数文字列を指定
		NULL,		//コールバック関数
		NULL		//コールバック関数が呼び出された時にデータが渡されるポインタ
	);

	//カーネルオブジェクトの作成
	//カーネルオブジェクトを作成する関数
	kernel = clCreateKernel(
		program,			//プログラムオブジェクト
		"phase_OpenCL",	//カーネルオブジェクト化するカーネル関数名
		NULL				//エラーコードを格納する変数
	);

	//メモリオブジェクトの作成（デバイス側でのメモリの割り当て）
	//デバイス上に作られるメモリ領域をホスト側で管理するためのオブジェクト
	d_hologram = clCreateBuffer(
		context,			//OpenCLコンテキスト
		CL_MEM_READ_WRITE,	//バッファオブジェクトの確保に使用されるメモリ領域やバッファオブジェクトの使われ方を指定するビットフィールド
		mem_size_h,			//バッファオブジェクトのサイズをバイトで指定
		NULL,				//アプリケーションが確保済みのバッファデータ
		NULL				//エラーコードを格納する変数
	);
	d_pls_x = clCreateBuffer(context, CL_MEM_READ_ONLY, mem_size_o, NULL, NULL);
	d_pls_y = clCreateBuffer(context, CL_MEM_READ_ONLY, mem_size_o, NULL, NULL);
	d_pls_z = clCreateBuffer(context, CL_MEM_READ_ONLY, mem_size_o, NULL, NULL);
	/*--------------------------------------------------*/


	/*--------------------メイン処理--------------------*/
	//デバイスメモリへのデータ転送
	//ホストメモリからバッファオブジェクトへの書き込みを行う
	clEnqueueWriteBuffer(
		command_queue,		//書き込みコマンドを挿入するコマンドキュー
		d_pls_x,			//バッファオブジェクト(デバイス側のメモリ)
		CL_TRUE,			//ブロッキングかノンブロッキングのどっちかで書き込むか指定
		0,					//(デバイス側のメモリへの)書き込み開始位置をずらすバイトで指定
		mem_size_o,			//書き込むデータのバイト数
		pls_x,				//データの読み込み元のホストメモリバッファへのポインタを指定
		0,					//イベントオブジェクトの数
		NULL,				//このコマンドが実行される前に完了していなければならないイベントを指定
		NULL				//この書き込みコマンドを識別するイベントオブジェクトが返される
	);
	clEnqueueWriteBuffer(command_queue, d_pls_y, CL_TRUE, 0, mem_size_o, pls_y, 0, NULL, NULL);
	clEnqueueWriteBuffer(command_queue, d_pls_z, CL_TRUE, 0, mem_size_o, pls_z, 0, NULL, NULL);

	//カーネル関数の引数設定
	//カーネルの特定の引数に値をセットする
	clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&d_pls_x);
	clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&d_pls_y);
	clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&d_pls_z);
	clSetKernelArg(
		kernel,				//値をセットするカーネル
		3,					//引数のインデックス
		sizeof(cl_mem),		//引数として渡すデータのサイズ
		(void *)&d_hologram	//第２引数で指定した引数にわたすデータへのポインタ
	);

	//ワークグループのサイズの定義
	global_size[0]	= width;
	global_size[1]	= heigth;
	local_size[0]	= 32;
	local_size[1]	= 32;

	//カーネル関数の実行
	//カーネルを実行するための関数
	status = clEnqueueNDRangeKernel(
		command_queue,	//コマンドキュー
		kernel,			//カーネルオブジェクト
		2,				//グローバルワークアイテム数とワークグループ内のワークアイテム数を決定する際の次元数
		NULL,			//現時点では必ずNULL
		global_size,	//カーネル関数を実行するwork_dim次元のグローバルワークアイテム(block)の数
		local_size,		//カーネル関数を実行する各ワークグループを構成するワークアイテム(thread)の数
		0,				//イベントオブジェクトの数
		NULL,			//このコマンドが実行される前に完了していなければならないイベントを指定
		&event			//この書き込みコマンドを識別するイベントオブジェクトが返される
	);

	//時間計測
	//イベントオブジェクトに関連付けられたコマンドが完了するのをホストスレッドで待つ関数
	status = clWaitForEvents(
		1,		//イベントオブジェクトの数
		&event	//完了を待つイベントオブジェクトのリスト
	);
	//eventと関連付けられたコマンドのプロファイリング情報を取得する関数
	status = clGetEventProfilingInfo(
		event,						//有効なイベントオブジェクト
		CL_PROFILING_COMMAND_START,	//取得する情報
		sizeof(cl_ulong),			//第4引数のメモリのサイズのバイト数
		&start,						//第2引数で指定した情報についての値が返されるメモリ空間へのポインタ
		NULL						//第4引数にコピーされるデータの実際のサイズをバイトで返す
	);
	status = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(ulong), &end, NULL);
	printf("calculation time\t%f ms\n", (end-start)*1e-06);

	//デバイス側からホスト側へのデータ転送
	//バッファオブジェクトからホストメモリへの読み込みを行う
	clEnqueueReadBuffer(
		command_queue,	//読み込みコマンドを挿入するコマンドキュー
		d_hologram,		//バッファオブジェクト(デバイス側のメモリ)
		CL_TRUE,		//ブロッキングかノンブロッキングのどっちかで読み込むか指定
		0,				//(デバイス側のメモリの)読み込み開始位置をずらすバイトで指定
		mem_size_h,		//書き込むデータのバイト数
		hologram,		//データの読み込み元のホストメモリバッファへのポインタを指定
		0,				//イベントオブジェクトの数
		NULL,			//このコマンドが実行される前に完了していなければならないイベントを指定
		NULL			//この書き込みコマンドを識別するイベントオブジェクトが返される
	);
	/*--------------------------------------------------*/


	/*--------------------終了処理--------------------*/
	clFinish(command_queue);
	clReleaseCommandQueue(command_queue);
	clReleaseContext(context);
	clReleaseMemObject(d_pls_x);
	clReleaseMemObject(d_pls_y);
	clReleaseMemObject(d_pls_z);
	clReleaseMemObject(d_hologram);
	clReleaseKernel(holo_calculation);
	clReleaseDevice(device_id);
	/*--------------------------------------------------*/

	min = hologram[0];
	max = hologram[0];
	//最大値，最小値を求める
	for (i=0; i<pixel; i++) {
		if (min>hologram[i]) {
			min = hologram[i];
		}
		if (max<hologram[i]) {
			max = hologram[i];
		}
	}
	mid = (min+max)/2;	//中間値（閾値）を求める
	printf("max=%d\tmin=%d\tmid=%d\n", max, min, mid);
	//各々の光強度配列の値を中間値と比較し，2値化する
	for (i=0; i<pixel; i++) {
		if (hologram[i]<mid) {
			hologram[i] = 0;
		}
		else {
			hologram[i] = 255;
		}
	}


	/*--------------------BMP関係--------------------*/
	BITMAPFILEHEADER bmpFh;
	BITMAPINFOHEADER bmpIh;
	RGBQUAD rgbQ[256];
	//BITMAPFILEHEADERの構造体
	bmpFh.bfType		=19778;	//'B'=0x42,'M'=0x4d,'BM'=0x4d42=19778
	bmpFh.bfSize		=14+40+1024+pixel;	//1024はカラーパレットのサイズ．256階調で4 byte一組
	bmpFh.bfReserved1	=0;
	bmpFh.bfReserved2	=0;
	bmpFh.bf0ffBits		=14+40+1024;
	//BITMAPINFOHEADERの構造体
	bmpIh.biSize			=40;
	bmpIh.biWidth			=width;
	bmpIh.biHeight			=heigth;
	bmpIh.biPlanes			=1;
	bmpIh.biBitCount		=8;
	bmpIh.biCompression		=0;
	bmpIh.biSizeImage		=0;
	bmpIh.biXPelsPerMeter	=0;
	bmpIh.biYPelsPerMeter	=0;
	bmpIh.biCirUsed			=0;
	bmpIh.biCirImportant	=0;
	//RGBQUADの構造体
	for (i=0; i<256; i++) {
		rgbQ[i].rgbBlue		=i;
		rgbQ[i].rgbGreen	=i;
		rgbQ[i].rgbRed		=i;
		rgbQ[i].rgbReserved	=0;
	}
	/*--------------------------------------------------*/


	fp = fopen("phase_clgpu.bmp","wb");		//宣言したfpと使用するファイル名，その読み書きモードを設定．バイナリ(b)で書き込み(w)
	fwrite(&bmpFh, sizeof(bmpFh), 1, fp);	//書き込むデータのアドレス，データのサイズ，データの個数，ファイルのポインタを指定
	fwrite(&bmpIh, sizeof(bmpIh), 1, fp);	//(&bmpFh.bfType, sizeof(bmpFh.bfType), 1, fp);というように個別に書くことも可能
	fwrite(&rgbQ[0], sizeof(rgbQ[0]), 256, fp);
	fwrite(hologram, sizeof(unsigned char), pixel, fp);	//bmpに書き込み
	printf("'phase_clgpu.bmp' was saved.\n\n");
	fclose(fp);

	return 0;
}
