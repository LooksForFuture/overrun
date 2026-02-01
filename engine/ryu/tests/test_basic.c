/*
  This test unit is for testing the deterministic behavior of the library
 */

#include <ryu/ryu.h>
#include <ryu/init.h>

#include <assert.h>

#define CREATE_WORLD(i, g) (((uint32_t)(i) << 8) | (g))
#define CREATE_ENTITY(i, g, w) (((uint64_t)(i) << 32)|((uint16_t)(g) << 16)|(w))

#define INIT_WORLD_CHECK(world, index, generation) \
assert(RYU_WORLD_INDEX(world) == index); \
assert(RYU_WORLD_GENERATION(world) == generation); \
	assert(ryu_isWorldValid(world));

#define INIT_ENTITY_CHECK(entity, index, generation, world) \
assert(RYU_ENTITY_INDEX(entity) == index); \
assert(RYU_ENTITY_GENERATION(entity) == generation); \
assert(RYU_ENTITY_WORLD(entity) == world); \
assert(ryu_isEntityValid(entity)); \
	assert(!ryu_isEntityPending(entity));

#define IMMEDIATE_DESTROY(entity, world) ryu_destroyEntity(entity); \
	ryu_flush(world);

int main(void)
{
	ryu_init();

	/* test worlds */
	RyuWorld world = ryu_newWorld();
	INIT_WORLD_CHECK(world, 0, 1)

	RyuWorld world2 = ryu_newWorld();
	INIT_WORLD_CHECK(world2, 1, 1);

	RyuWorld world3 = ryu_newWorld();
	INIT_WORLD_CHECK(world3, 2, 1);

	ryu_destroyWorld(world2);
	assert(!ryu_isWorldValid(world2));

	world2 = ryu_newWorld();
	INIT_WORLD_CHECK(world2, 1, 2);

	RyuWorld world4 = ryu_newWorld();
	INIT_WORLD_CHECK(world4, 3, 1);

	ryu_destroyWorld(world3);
	assert(!ryu_isWorldValid(world3));
	ryu_destroyWorld(world);
	assert(!ryu_isWorldValid(world));

	world = ryu_newWorld();
	INIT_WORLD_CHECK(world, 0, 2);

	world3 = ryu_newWorld();
	INIT_WORLD_CHECK(world3, 2, 2);

	ryu_destroyWorld(world2);
	assert(!ryu_isWorldValid(world2));
	ryu_destroyWorld(world4);
	assert(!ryu_isWorldValid(world4));

	world4 = ryu_newWorld();
	INIT_WORLD_CHECK(world4, 3, 2);

	world2 = ryu_newWorld();
	INIT_WORLD_CHECK(world2, 1, 3);

	for (int i = 0; i < 32; i++) {
		ryu_newWorld();
	}

	assert(!ryu_isWorldValid(CREATE_WORLD(255, 999)));

	/* test entities */
	Entity ent1_1 = ryu_newEntity(world);
	INIT_ENTITY_CHECK(ent1_1, 0, 1, world);

	Entity ent2_1 = ryu_newEntity(world);
	INIT_ENTITY_CHECK(ent2_1, 1, 1, world);

	Entity ent3_1 = ryu_newEntity(world);
	INIT_ENTITY_CHECK(ent3_1, 2, 1, world);

	IMMEDIATE_DESTROY(ent1_1, world);
	assert(!ryu_isEntityValid(ent1_1));
	IMMEDIATE_DESTROY(ent3_1, world);
	assert(!ryu_isEntityValid(ent3_1));
	
	ent3_1 = ryu_newEntity(world);
	INIT_ENTITY_CHECK(ent3_1, 2, 2, world);

	ent1_1 = ryu_newEntity(world);
	INIT_ENTITY_CHECK(ent1_1, 0, 2, world);

	IMMEDIATE_DESTROY(ent2_1, world);
	assert(!ryu_isEntityValid(ent2_1));
	IMMEDIATE_DESTROY(ent1_1, world);
	assert(!ryu_isEntityValid(ent1_1));

	ent1_1 = ryu_newEntity(world);
	INIT_ENTITY_CHECK(ent1_1, 0, 3, world);

	ent2_1 = ryu_newEntity(world);
	INIT_ENTITY_CHECK(ent2_1, 1, 2, world);

	assert(!ryu_isEntityPending(ent2_1));
	ryu_destroyEntity(ent2_1);
	assert(ryu_isEntityValid(ent2_1));
	assert(ryu_isEntityPending(ent2_1));
	ryu_destroyEntity(ent2_1); //shouldn't have a problem

	ryu_flush(world);

	assert(!ryu_isEntityValid(ent2_1));
	assert(!ryu_isEntityPending(ent2_1));
	ryu_destroyEntity(ent2_1);
	assert(!ryu_isEntityValid(ent2_1));
	assert(!ryu_isEntityPending(ent2_1));

	assert(!ryu_isEntityValid(CREATE_ENTITY(264, 0, world)));

	ryu_shutdown();
	return 0;
}
