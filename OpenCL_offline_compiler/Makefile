all: oclc

oclc: oclc.cpp
	clang++ -std=c++11 -I/usr/local/cuda-8.0/include -Wall -g oclc.cpp -o oclc -L/usr/local/cuda-8.0/lib64 -lOpenCL

clean:
	rm oclc
