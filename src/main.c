#include <ecs.h>

#include <assert.h>
#include <stdio.h>

// --- Components
typedef struct { float x, y; } Transform;
typedef struct { float vx, vy; } Velocity;
typedef struct { int sprite; } Render;

ECS_DECL_COMP(Transform);
ECS_DECL_COMP(Velocity);
ECS_DECL_COMP(Render);

Archetype *ship;
Archetype *bullet;

int main(void)
{
	ecs_init();

	ECS_REG_COMP(Transform);
	ECS_REG_COMP(Velocity);
	ECS_REG_COMP(Render);

	ship = ECS_REG_ARCH(Transform, Render);
	assert(ship != NULL);
	bullet = ECS_REG_ARCH(Transform, Render, Velocity);
	assert(bullet != NULL);

	Entity ent = ecs_newEntityInArch(ship);
	assert(ecs_getEntityArch(ent) == ship);

	Velocity *v = ECS_ADD_COMPONENT(ent, Velocity);
	assert(ecs_getEntityArch(ent) == bullet);
	assert(ECS_ADD_COMPONENT(ent, Velocity) == v);

	ecs_destroyEntity(ent);
	assert(!ecs_isValid(ent));
	assert(ecs_getEntityArch(ent) == NULL);

	ent = ecs_newEntityInArch(ship);
	Transform *t = ECS_ADD_COMPONENT(ent, Transform);
	t->x = 112.0f;
	t->y = 99.0f;

	EcsQuery *q = ECS_QUERY(ECS_ACCESS(include, Transform, Render));

	EcsIter it = ecs_queryIter(q);
	while (ecs_iterNext(&it)) {
		Entity ent = it.entity;
		Transform *t = ECS_ADD_COMPONENT(ent, Transform);
		printf("%f %f\n", t->x, t->y);
	}

	ecs_shutdown();
}
