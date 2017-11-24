#include <stdio.h>
#include <math.h>
#include <stdint.h>		//uint32_tは符号なしintで4バイトに指定
#include <stdlib.h> 	//記憶域管理を使うため
#include <CL/cl.h>

#define width 1024
#define heigth 1024
#define pixel width*heigth
#define points 284

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
	uint32_t		biWidth;			//横の画素数
	uint32_t		biHeight;			//縦の画素数
	unsigned short	biPlanes;			//1
	unsigned short	biBitCount;			//一画素あたりの色の数のbit数．今回は8
	uint32_t		biCompression;		//圧縮タイプを表す．bmpは非圧縮なので0
	uint32_t		biSizeImage;		//bmp配列のサイズを表す．biCompression=0なら基本的に0
	uint32_t		biXPelsPerMeter;	//biXPelsPerMeterとbiYPelsPerMeterは基本的に0
	uint32_t		biYPelsPerMeter;
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


/*画像生成用の配列*/
unsigned char hologram[pixel];	//光強度用の配列
unsigned char img[pixel];		//bmp用の配列


/*--------------------main関数--------------------*/
int main(){
	BITMAPFILEHEADER bmpFh;
	BITMAPINFOHEADER bmpIh;
	RGBQUAD rgbQ[256];

	/*ホスト側の変数*/
	int i;					//物体点
	float min=0.0F, max=0.0F, mid;	//2値化に用いる
	FILE *fp, *fp1;
	int pls_x[points];			//~~データを読み込むことで初めてこの配列が定義できる~~
	int pls_y[points];
	float pls_z[points];
	int x_buf, y_buf, z_buf;	//データを一時的に溜めておくための変数

	/*BITMAPFILEHEADERの構造体*/
	bmpFh.bfType		=19778;	//'B'=0x42,'M'=0x4d,'BM'=0x4d42=19778
	bmpFh.bfSize		=14+40+1024+pixel;	//1024はカラーパレットのサイズ．256階調で4 byte一組
	bmpFh.bfReserved1	=0;
	bmpFh.bfReserved2	=0;
	bmpFh.bf0ffBits		=14+40+1024;

	/*BITMAPINFOHEADERの構造体*/
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

	/*RGBQUADの構造体*/
	for(i=0; i<256; i++){
		rgbQ[i].rgbBlue		=i;
		rgbQ[i].rgbGreen	=i;
		rgbQ[i].rgbRed		=i;
		rgbQ[i].rgbReserved	=0;
	}

	/*3Dファイルの読み込み*/
	fp=fopen("cube284.3d","rb");		//バイナリで読み込み
	if (!fp) {
		printf("error!\n");
		exit(1);
	}
/*	fread(&points, sizeof(int), 1, fp);	//データのアドレス，サイズ，個数，ファイルポインタを指定
	printf("the number of points is %d\n", points);

    //取り出した物体点を入れる配列
	int pls_x[points];			//~~データを読み込むことで初めてこの配列が定義できる~~
	int pls_y[points];
	float pls_z[points];
	int x_buf, y_buf, z_buf;	//データを一時的に溜めておくための変数
*/
	/*各バッファに物体点座標を取り込み，ホログラム面と物体点の位置を考慮したデータを各配列に入れる*/
	for(i=0; i<points; i++){
		fread(&x_buf, sizeof(int), 1, fp);
		fread(&y_buf, sizeof(int), 1, fp);
		fread(&z_buf, sizeof(int), 1, fp);

		pls_x[i]=x_buf*40+width*0.5;	//物体点を離すために物体点座標に40を掛け，中心の座標を足す
		pls_y[i]=y_buf*40+heigth*0.5;
		pls_z[i]=1.0F/(((float)z_buf)*40+10000.0F);
	}
	fclose(fp);

	for (i=0; i<pixel; i++) {
		hologram[i]=0;
	}

	int mem_size_h = sizeof(unsigned char)*pixel;
	int mem_size_o = sizeof(float)*points;

/*--------------------OpenCL変数宣言--------------------*/
	/*OpenCL変数*/
	cl_device_id		device_id		= NULL;
	cl_context			context			= NULL;
	cl_command_queue	command_queue	= NULL;
	cl_program			program			= NULL;
	cl_kernel			kernel			= NULL;
	cl_platform_id		platform_id		= NULL;

	/*カーネル実行用変数*/
	size_t	global_size[2];
	size_t	local_size[2];
	char	*source	= 0;
	size_t	source_size	= 0;

	/*メモリオブジェクト（デバイス側で結果を格納する変数）*/
	cl_mem	d_hologram	= NULL;
	cl_mem	d_pls_x		= NULL;
	cl_mem	d_pls_y		= NULL;
	cl_mem	d_pls_z		= NULL;

	/*カーネルオブジェクト*/
	cl_kernel	holo_calculation	= NULL;
/*--------------------------------------------------*/


/*--------------------OpenCLの初期化--------------------*/
	/*プラットフォームの取得*/
	//プラットフォームはそれぞれ固有のIDを持つため、使用できるプラットフォームのIDを取得する関数
	//プラットフォーム数, プラットフォームIDを格納する変数（複数なら配列）, 利用できるプラットフォーム数の戻り地
	clGetPlatformIDs(1, &platform_id, NULL);

	/*デバイスの取得*/
	//デバイスも固有のIDを持つため、使用できるデバイスのIDを取得する関数
	//clGetPlatformIDs関数で取得したプラットフォームID, 利用したいデバイスの種類, 利用するデバイス数, cl_device_id型のハンドル(ポインタ), 第2引数で指定したデバイス数の戻り地
	clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, 1, &device_id, NULL);

	/*コンテキストの作成*/
	//いくつかのデバイスやメモリをまとめて管理するためにコンテキスト環境を作成する関数
	//コンテキストプロパティの指定, 第3引数のデバイス数, clGetDeviceIDs関数で取得されたデバイス, コールバック関数, コールバック関数が呼び出された時にデータが渡されるポインタ, エラーコード
	context = clCreateContext(NULL, 1, &device_id, NULL, NULL, NULL);

	/*コマンドキューの作成*/
	//デバイスとホストをつなげるコマンドキューを作成する関数
	//OpenCLコンテキスト, 第１引数に関連づけられたデバイス(コマンドを実行するデバイス), コマンドキューに適用するプロパティのリスト, エラーコードを格納する変数
	command_queue = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, NULL);

	/*ソースコードの読み込み*/
	//OpenCLではカーネルソースに記載されてるカーネル関数群をプログラムオブジェクトという単位で管理する
	fp = fopen("holo_calculation_OpenCL.cl", "r");
	if (!fp) {
		printf("error!\n");
		exit(1);
	}
	source = (char *)malloc(0x100000);
	source_size = fread(source, 1, 0x100000, fp);
	fclose(fp);

	/*プログラムオブジェクトの作成*/
	//読み込んだカーネルソースからプログラムオブジェクトを作成する関数
	//正常なOpenCLコンテキストを引き渡す(必ずやる), 第３引数のデバイス数, カーネルプログラムのソース, ソースの文字列のサイズを指定, エラーコードを格納する変数
	program = clCreateProgramWithSource(context, 1, (const char**)&source, (const size_t *)&source_size, NULL);

	/*カーネル関数のビルド*/
	//作成したプログラムオブジェクトからカーネル関数をビルドする
	//ビルド対象のプログラムオブジェクト, 第3引数で与えたデバイス数, プログラムオブジェクトに関連付けられたデバイスのリストへのポインタを指定, コンパイラに渡す引数文字列を指定, コールバック関数, コールバック関数が呼び出された時にデータが渡されるポインタ
	clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);

	/*カーネルオブジェクトの作成*/
	//カーネルオブジェクトを作成する関数
	//プログラムオブジェクト, カーネルオブジェクト化するカーネル関数名, エラーコードを格納する変数
	holo_calculation = clCreateKernel(program, "holo_calculation_OpenCL", NULL);

	/*メモリオブジェクトの作成（デバイス側でのメモリの割り当て）*/
	//デバイス上に作られるメモリ領域をホスト側で管理するためのオブジェクト
	//OpenCLコンテキスト, バッファオブジェクトの確保に使用されるメモリ領域やバッファオブジェクトの使われ方を指定するビットフィールド, バッファオブジェクトのサイズをバイトで指定, アプリケーションが確保済みのバッファデータ, エラーコードを格納する変数
	d_pls_x		= clCreateBuffer(context, CL_MEM_READ_ONLY, mem_size_o, NULL, NULL);
	d_pls_y		= clCreateBuffer(context, CL_MEM_READ_ONLY, mem_size_o, NULL, NULL);
	d_pls_z		= clCreateBuffer(context, CL_MEM_READ_ONLY, mem_size_o, NULL, NULL);
	d_hologram	= clCreateBuffer(context, CL_MEM_READ_WRITE, mem_size_h, NULL, NULL);
/*--------------------------------------------------*/


/*--------------------メイン処理--------------------*/
	/*デバイスメモリへのデータ転送*/
	//ホストメモリからバッファオブジェクトへの書き込みを行う
	//書き込みコマンドを挿入するコマンドキュー. バッファオブジェクト(デバイス側のメモリ), ブロッキングかノンブロッキングのどっちかで書き込むか指定, (デバイス側のメモリへの)書き込み開始位置をずらすバイトで指定, 書き込むデータのバイト数, データの読み込み元のホストメモリバッファへのポインタを指定, イベントオブジェクトの数, このコマンドが実行される前に完了していなければならないイベントを指定, この書き込みコマンドを識別するイベントオブジェクトが返される
	clEnqueueWriteBuffer(command_queue, d_pls_x, CL_TRUE, 0, mem_size_o, pls_x, 0, NULL, NULL);
	clEnqueueWriteBuffer(command_queue, d_pls_y, CL_TRUE, 0, mem_size_o, pls_y, 0, NULL, NULL);
	clEnqueueWriteBuffer(command_queue, d_pls_z, CL_TRUE, 0, mem_size_o, pls_z, 0, NULL, NULL);

	/*カーネル関数の引数設定*/
	//カーネルの特定の引数に値をセットする
	//値をセットするカーネル, 引数のインデックス, 引数として渡すデータのサイズ, 第２引数で指定した引数にわたすデータへのポインタ
	clSetKernelArg(holo_calculation, 0, sizeof(cl_mem), (void *)&d_pls_x);
	clSetKernelArg(holo_calculation, 1, sizeof(cl_mem), (void *)&d_pls_y);
	clSetKernelArg(holo_calculation, 2, sizeof(cl_mem), (void *)&d_pls_z);
	clSetKernelArg(holo_calculation, 3, sizeof(cl_mem), (void *)&d_hologram);

	/*ワークグループのサイズの定義*/
	global_size[0]	= width;
	global_size[1]	= heigth;
	local_size[0]	= 32;
	local_size[1]	= 32;

	/*カーネル関数の実行*/
	//カーネルを実行するための関数
	//有効なコマンドキュー, カーネルオブジェクト, グローバルワークアイテム数とワークグループ内のワークアイテム数を決定する際の次元数, 現時点では必ずNULL, カーネル関数を実行するwork_dim次元のグローバルワークアイテムの数, カーネル関数を実行する各ワークグループを構成するワークアイテムの数, イベントオブジェクトの数, このコマンドが実行される前に完了していなければならないイベントを指定, この書き込みコマンドを識別するイベントオブジェクトが返される
	clEnqueueNDRangeKernel(command_queue, holo_calculation, 2, NULL, global_size, local_size, 0, NULL, NULL);

	/*デバイス側からホスト側へのデータ転送*/
	//バッファオブジェクトからホストメモリへの読み込みを行う
	//読み込みコマンドを挿入するコマンドキュー, バッファオブジェクト(デバイス側のメモリ), ブロッキングかノンブロッキングのどっちかで読み込むか指定, (デバイス側のメモリの)読み込み開始位置をずらすバイトで指定, 書き込むデータのバイト数, データの読み込み元のホストメモリバッファへのポインタを指定, イベントオブジェクトの数, このコマンドが実行される前に完了していなければならないイベントを指定, この書き込みコマンドを識別するイベントオブジェクトが返される
	clEnqueueReadBuffer(command_queue, d_hologram, CL_TRUE, 0, mem_size_h, hologram, 0, NULL, NULL);
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


	/*最大値，最小値を求める*/
	for(i=0; i<pixel; i++){
		if(min>hologram[i]){
			min=hologram[i];
		}
		if(max<hologram[i]){
			max=hologram[i];
		}
	}
	mid=(min+max)*0.5F;	//中間値（閾値）を求める
	printf("min=%lf, max=%lf, mid=%lf\n", min, max, mid);

	/*各々の光強度配列の値を中間値と比較し，2値化する*/
	for(i=0; i<pixel; i++){
		if(hologram[i]<mid){
			img[i]=0;
		}
		else{
			img[i]=255;
		}
	}

	/*宣言したfpと使用するファイル名，その読み書きモードを設定．バイナリ(b)で書き込み(w)*/
	fp1=fopen("fresnel-gpu.bmp","wb");

	/*書き込むデータのアドレス，データのサイズ，データの個数，ファイルのポインタを指定*/
	fwrite(&bmpFh, sizeof(bmpFh), 1, fp1);	//(&bmpFh.bfType, sizeof(bmpFh.bfType), 1, fp);というように個別に書くことも可能
	fwrite(&bmpIh, sizeof(bmpIh), 1, fp1);
	fwrite(&rgbQ[0], sizeof(rgbQ[0]), 256, fp1);
	fwrite(img, sizeof(unsigned char), pixel, fp1);	//bmpに書き込み
	printf("'fresnel-gpu.bmp' was saved.\n\n");
	fclose(fp1);

	return 0;
}
