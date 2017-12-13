all: make_ptx

make_ptx: make_ptx.c
	gcc -Wall -g make_ptx.c -o make_ptx.out -I/usr/local/cuda-8.0/include -L/usr/local/cuda-8.0/lib64 -lOpenCL

clean:
	rm make_ptx.out
	rm cl_kernel.ptx
