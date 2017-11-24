all: fresnelCL

fresnelCL: fresnel-clgpu.c
	gcc -I/usr/local/cuda-8.0/include -o fresnel.out fresnel-clgpu.c -L/usr/local/cuda-8.0/lib64 -lOpenCL

clean:
	rm fresnel.out fresnel-gpu.bmp    
