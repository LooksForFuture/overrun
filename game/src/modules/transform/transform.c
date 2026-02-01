#include <modules/transform/transform.h>
#include <modules/transform/reg_module.h>

#include <ryu/ryu.h>
#include <ryu/component.h>

#include <stdint.h>

int transform__id = 0;

void transform_regModule()
{
	transform__id = ryu_regComponent();
}
