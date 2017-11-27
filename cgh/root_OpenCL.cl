#define width 2048
#define heigth 2048
#define points 284
#define PI 3.14159265F
#define SAMPLING_INTERVAL 10.5F			//画素間隔
#define WAVE_LENGTH 0.633F				//光波長
#define WAVE_NUMBER 2.0F*PI/WAVE_LENGTH	//波数


/*直接計算のカーネル関数*/
__kernel void root_OpenCL(__global float *o_x, __global float *o_y, __global float *o_z, __global float *h) {
	//ホログラム面上の座標Hx,Hyを組み込み変数により定義
	int x   = get_global_id(0);
	int y   = get_global_id(1);
	int adr = x+y*width;

	int k;
	float xx, yy, zz, rr, cnst;
	cnst = SAMPLING_INTERVAL*WAVE_NUMBER;

	for (k=0; k<284; k++) {
		xx = ((float)x-o_x[k])*((float)x-o_x[k]);
		yy = ((float)y-o_y[k])*((float)y-o_y[k]);
		zz = o_z[k]*o_z[k];
		rr = sqrt(xx+yy+zz);
		h[adr] = h[adr]+native_cos(cnst*rr);
	}
}
