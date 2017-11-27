all: clgpu root fresnel

clgpu: clgpu.c
	gcc -I/usr/local/cuda-8.0/include -o clgpu.out clgpu.c -L/usr/local/cuda-8.0/lib64 -lOpenCL
root: root_clgpu.c
	gcc -I/usr/local/cuda-8.0/include -o root_clgpu.out root_clgpu.c -L/usr/local/cuda-8.0/lib64 -lOpenCL
fresnel: fresnel_clgpu.c
	gcc -I/usr/local/cuda-8.0/include -o fresnel_clgpu.out fresnel_clgpu.c -L/usr/local/cuda-8.0/lib64 -lOpenCL

clean:
	rm clgpu.out clgpu.bmp root_clgpu.out root_clgpu.bmp fresnel_clgpu.out fresnel_clgpu.bmp
