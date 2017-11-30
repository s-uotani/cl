all: phase root fresnel

phase: phase_clgpu.c
	gcc -I/usr/local/cuda-8.0/include -o phase_clgpu.out phase_clgpu.c -L/usr/local/cuda-8.0/lib64 -lOpenCL
fresnel: fresnel_clgpu.c
	gcc -I/usr/local/cuda-8.0/include -o fresnel_clgpu.out fresnel_clgpu.c -L/usr/local/cuda-8.0/lib64 -lOpenCL
root: root_clgpu.c
	gcc -I/usr/local/cuda-8.0/include -o root_clgpu.out root_clgpu.c -L/usr/local/cuda-8.0/lib64 -lOpenCL

clean:
	rm phase_clgpu.out fresnel_clgpu.out root_clgpu.out 
	rm phase_clgpu.bmp fresnel_clgpu.bmp root_clgpu.bmp 


