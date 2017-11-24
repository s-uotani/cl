#define POINT_NUMBER 284
#define NORM_CONST 40.5845105F//�z���O�����K�i���p�̒萔
#define X_SIZE 1408
#define Y_SIZE 1080
#define PI2 6.28318530F
#define SAMPLING_INTERVAL 10.5F

__kernel void kernel_source_cube(__global float *o_x, __global float *o_y, __global float *o_z, __global unsigned char *h){

	//�z���O�����ʏ�̍��WxH,yH��g�ݍ��ݕϐ��ɂ���`
	int x = get_global_id(0);
	int y = get_global_id(1);
	int adr = x + y*X_SIZE;

	//�v�Z�ɗ��p����ϐ��̐錾�ƃp�����[�^�ݒ�
	int l;
	float theta, r_x, r_y;
	float d_x, d_y, d_z;
	float h_r = 0.0f;
	float h_i = 0.0f;
	float temp_h;

	//�z���O�����̌v�Z
	for (l = 0; l<POINT_NUMBER; l++){//���̓_�̃��[�v

		d_x = o_x[l];
		d_y = o_y[l];
		d_z = o_z[l];

		r_x = d_x - x;
		r_y = d_y - y;

		theta = SAMPLING_INTERVAL*(r_x*r_x + r_y*r_y) * d_z;

		h_r += native_cos(theta);
		h_i += native_sin(theta);

		temp_h = atan2(h_i, h_r);

		//�Ίp�̕␳
		if (temp_h < 0.0){
			temp_h += PI2;
		}

		//256�K���i8�r�b�g�j��
		h[adr] = (unsigned char)(temp_h * NORM_CONST);

	}
}
