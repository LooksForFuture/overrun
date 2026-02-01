/*
  This test unit tries to see if the library can handle so many random stuff
 */

#include <ryu/ryu.h>
#include <ryu/init.h>

#include <stdlib.h>
#include <time.h>

int main(void)
{
	srand(time(NULL));
	ryu_init();

	for (int i = 0; i < 255; i++) {
		switch(rand() % 3) {
		case 0: ryu_newWorld(); break;
		case 1: ryu_isWorldValid(rand() % UINT16_MAX); break;
		case 2: ryu_destroyWorld(rand() % UINT16_MAX); break;
		}
	}

	for (int i = 0; i < 100000; i++) {
		switch (rand() % 5) {
		case 0: ryu_newEntity(rand() % UINT16_MAX); break;
		case 1: ryu_destroyEntity(rand() % UINT64_MAX); break;
		case 2: ryu_flush(rand() % UINT16_MAX); break;
		case 3: ryu_isEntityValid(rand() % UINT64_MAX); break;
		case 4: ryu_isEntityPending(rand() % UINT64_MAX); break;
		}
	}

	ryu_shutdown();
	return 0;
}
