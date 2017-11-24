#define width 1024
#define heigth 1024
#define points 284
#define PI2 6.28318530F
#define NORM_CONST 40.5845105F

/*フレネル近似のカーネル関数
__kernel void holo_calculation_OpenCL(__global float *o_x, __global float *o_y, __global float *o_z, __global float *h) {
    //ホログラム面上の座標Hx,Hyを組み込み変数により定義
    int x   = get_global_id(0);
    int y   = get_global_id(1);
    int adr = x+y*width;

    //計算に利用する変数の宣言とパラメータ設定
    int k;
    float d_x, d_y, d_z;
    float r_x, r_y;
	float xx, yy, theta;
    float h_r = 0.0F;
    float h_i = 0.0F;
    float temp_h;

	for(k=0; k<points; k++){
        d_x = o_x[k];
        d_y = o_y[k];
        d_z = o_z[k];

        r_x = d_x-x;
        r_y = d_y-y;

        theta = (r_x*r_x + r_y*r_y)*d_z;

//		xx = ((float)x-o_x[k])*((float)x-o_x[k]);
//		yy = ((float)y-o_y[k])*((float)y-o_y[k]);
//      theta = (xx+yy)*o_z[k]);

        h_r += native_cos(theta);
        h_i += native_sin(theta);

        temp_h = atan2(h_i, h_r);

        //偏角の補正
        if (temp_h<0.0) {
            temp_h += PI2;
        }

        //8bit化
		h[adr]=(unsigned char)(h[adr]+native_cos(temp_h*NORM_CONST);
	}
}*/

__kernel void holo_calculation_OpenCL(__global float *o_x, __global float *o_y, __global float *o_z, __global float *h) {
    //ホログラム面上の座標Hx,Hyを組み込み変数により定義
    int x   = get_global_id(0);
    int y   = get_global_id(1);
    int adr = x+y*width;

    int k;
    float xx, yy, zz, rr;

    //計算に必要な変数の定義
    float interval=10.5F;				//画素間隔
    float wave_len=0.633F;				//光波長
    float wave_num=2.0*M_PI/wave_len;	//波数
    float cnst=interval*wave_num;

    for(k=0; k<284; k++){
        xx=((float)j-o_x[k])*((float)j-o_x[k]);
        yy=((float)i-o_y[k])*((float)i-o_y[k]);
        zz=o_z[k]*o_z[k];
        rr=sqrt(xx+yy+zz);
        h[adr]=h[adr]+native_cos(cnst*rr);
    }
}
