#include "ecs.h"

#include <stdio.h>

typedef struct {
	float x, y;
} Transform;

typedef struct {
	int index; //sprite index
} Render;

ECS_DECL_COMP(Transform);
ECS_DECL_COMP(Render);

Archetype *Ship;

int main(void)
{
	ecs_init();

	ECS_REG_COMP(Transform);
	ECS_REG_COMP(Render);

	Ship = ECS_REG_ARCH(Transform, Render);
	Entity ent1 = ecs_newEntity(Ship);
	printf("%ld\n", ent1);
	Entity ent2 = ecs_newEntity(Ship);
	printf("%ld\n", ent2);
	Entity ent3 = ecs_newEntity(Ship);
	printf("%ld\n", ent3);

	ecs_shutdown();
}
