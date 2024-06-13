#include <stdio.h>

union PreemptUnion {
	unsigned long long preempt_count; // 8 byte 
	struct {
		unsigned int count; // 4 byte
		unsigned int need_resched; // 4byte
	} preempt;
};

int main() {
	union PreemptUnion u;


	u.preempt_count = 9;
	printf("u.preempt_count = 9\n");
	printf("preempt_count : %llu\n", u.preempt_count);
	printf("upreempot.cout : %u\n", u.preempt.count);
	printf("upreempot.need_resched : %u\n", u.preempt.need_resched);
	printf("===========\n");

	u.preempt.count = 10;
	printf("u.preempt.count = 10\n");
	printf("preempt_count : %llu\n", u.preempt_count);
	printf("upreempot.cout : %u\n", u.preempt.count);
	printf("upreempot.need_resched : %u\n", u.preempt.need_resched);

	printf("===========\n");


	u.preempt.need_resched = 5;
	printf("u.preempt.need_resched = 5\n");
	printf("preempt_count : %llu\n", u.preempt_count);
	printf("upreempot.cout : %u\n", u.preempt.count);
	printf("upreempot.need_resched : %u\n", u.preempt.need_resched);

	printf("===========\n");


	return 0;
}
