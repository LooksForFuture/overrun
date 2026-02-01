#include <glut/glut.h>

#include <ryu/ryu.h>
#include <ryu/init.h>

#include <modules/transform/transform.h>
#include <modules/transform/reg_module.h>

#include <assert.h>
#include <stdio.h>

int main(void)
{
	glut_init();
	ryu_init();

	transform_regModule();

	ryu_shutdown();
	glut_shutdown();
	return 0;
}
