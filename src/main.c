#include <ryu/ryu.h>
#include <ryu/init.h>

#include <assert.h>
#include <stdio.h>

int main(void)
{
	ryu_init();
	ryu_shutdown();
	return 0;
}
