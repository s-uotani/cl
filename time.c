#include <stdio.h>
#include <sys/time.h>

double gettimeofday_sec(){
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec * 1e-6;
}

int main(void){
    double start, end;
    start=gettimeofday_sec();
    /*処理*/
    end=gettimeofday_sec();
    printf("time\t%lf s\n", end-start);
    return 0;
}
