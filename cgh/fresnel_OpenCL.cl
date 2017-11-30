#define width 1024
#define heigth 1024
#define points 284
#define PI 3.14159265F
#define WAVE_LENGTH 0.633F			//光波長


/*フレネル近似のカーネル関数*/
__kernel void fresnel_OpenCL(__global float *o_x, __global float *o_y, __global float *o_z, __global float *h) {
	//ホログラム面上の座標Hx,Hyを組み込み変数により定義
	int x   = get_global_id(0);
	int y   = get_global_id(1);
	int adr = x+y*width;

	int k;
	float xx, yy;
	float wave_num = PI/WAVE_LENGTH;	//波数

	for (k=0; k<284; k++) {
		xx = ((float)x-o_x[k])*((float)x-o_x[k]);
		yy = ((float)y-o_y[k])*((float)y-o_y[k]);
		h[adr] = h[adr]+native_cos(wave_num*(xx+yy)*o_z[k]);
	}
}
