#define width 2048
#define heigth 2048
#define points 284
#define PI2 6.28318530F
#define NORM_CONST 40.5845105F
#define SAMPLING_INTERVAL 10.5F

/*フレネル近似のカーネル関数*/
__kernel void holo_calculation_OpenCL(__global float *o_x, __global float *o_y, __global float *o_z, __global unsigned char *h) {
	//ホログラム面上の座標Hx,Hyを組み込み変数により定義
	int x   = get_global_id(0);
	int y   = get_global_id(1);
	int adr = x+y*width;

	//計算に利用する変数の宣言とパラメータ設定
	int k;
	float r_x, r_y, theta;
	float h_r = 0.0F;
	float h_i = 0.0F;
	float temp_h;

	for(k=0; k<points; k++){
		r_x = o_x[k]-(float)x;
		r_y = o_y[k]-(float)y;

		theta = SAMPLING_INTERVAL*(r_x*r_x + r_y*r_y)*o_z[k];

		h_r += native_cos(theta);
		h_i += native_sin(theta);

		temp_h = atan2(h_i, h_r);

		//偏角の補正
		if (temp_h<0.0) {
			temp_h += PI2;
		}

		//8bit化
		h[adr]=(unsigned char)(temp_h*NORM_CONST);
	}
}
