all: hello

hello: hello.c
	gcc -I/usr/local/cuda-8.0/include -o hello.out hello.c -L/usr/local/cuda-8.0/lib64 -lOpenCL

clean:
	rm hello.out   
