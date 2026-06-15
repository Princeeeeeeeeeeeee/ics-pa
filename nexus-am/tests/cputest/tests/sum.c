#include "trap.h"

int main() {
	int i = 1;
	volatile int sum = 0;
	while(i <= 4) {
		sum += i;
		i ++;
	}

	nemu_assert(sum == 10);

	return 0;
}
