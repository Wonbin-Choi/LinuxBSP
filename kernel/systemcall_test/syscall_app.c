#include <stdio.h>
#include <unistd.h>
#include <asm-generic/unistd.h>
#pragma GCC diagnostic ignored "-Wunused-result"

int main(){
	long val;
	printf("input val = ");
	scanf("%ld",&val);
	val = syscall(__NR_mysyscall,val);
	if(val<0){
		perror("syscall");
		return 1;
	}
	printf("mysyscall return val = %ld\n",val);
	return 0;
}
