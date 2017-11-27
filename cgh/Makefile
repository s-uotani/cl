all: clgpu

clgpu: clgpu.c
	gcc -I/usr/local/cuda-8.0/include -o clgpu.out clgpu.c -L/usr/local/cuda-8.0/lib64 -lOpenCL

clean:
	rm clgpu.out clgpu.bmp
