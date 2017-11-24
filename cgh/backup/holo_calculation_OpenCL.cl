#define width 1024
#define heigth 1024
#define points 284
#define PI 3.14159265F

/*フレネル近似のカーネル関数*/
__kernel void holo_calculation_OpenCL(__global float *d_pls_x, __global float *d_pls_y, __global float *d_pls_z, __global float *d_hologram) {
    //ホログラム面上の座標Hx,Hyを組み込み変数により定義
    int x   = get_global_id(0);
    int y   = get_global_id(1);
    int adr = x+y*width;

    //計算に利用する変数の宣言とパラメータ設定
    int k;
	float xx, yy;

	float wave_len=0.633F;         //光波長
	float wave_num=PI/wave_len;  //波数の2分の1

	for(k=0; k<points; k++){
		xx=((float)x-d_pls_x[k])*((float)x-d_pls_x[k]);
		yy=((float)y-d_pls_y[k])*((float)y-d_pls_y[k]);
		d_hologram[adr]=d_hologram[adr]+native_cos(wave_num*(xx+yy)*d_pls_z[k]);
	}
}
