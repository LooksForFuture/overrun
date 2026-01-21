#include <ryu/ryu.h>
#include <ryu/init.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef RYU_INIT_WORLD_COUNT
#define RYU_INIT_WORLD_COUNT 8
#endif

#ifndef RYU_INIT_ENTITY_COUNT
#define RYU_INIT_ENTITY_COUNT 64
#endif

#define CREATE_WORLD(i, g) (((uint32_t)(i) << 8) | (g))
#define CREATE_ENTITY(i, g, w) (((uint64_t)(i) << 32)|((uint16_t)(g) << 16)|(w))

typedef struct {
	Entity id;
	bool pending;
} EntityDesc;

typedef struct {
	RyuWorld id;
	uint32_t entityCount;
	EntityDesc *entities;
	uint32_t stagedEntity;
} World;

//count of allocated worlds
static uint8_t worldCount = 0;
static World *worlds = NULL;
static uint8_t stagedWorld = 0;

void ryu_init(void)
{
	worlds = malloc(RYU_INIT_WORLD_COUNT * sizeof(World));
	worldCount = RYU_INIT_WORLD_COUNT;

	for (uint8_t i = 0; i < worldCount - 1; i++) {
		worlds[i].id = CREATE_WORLD(i+1, 0);
		worlds[i].entityCount = 0;
		worlds[i].entities = NULL;
		worlds[i].stagedEntity = 0;
	}
	worlds[worldCount].id = CREATE_WORLD(UINT8_MAX, 0);
	stagedWorld = 0;
}

void ryu_shutdown(void)
{
	for (uint8_t i = 0; i < worldCount; i++) {
		World *world = &worlds[i];
		free(world->entities);
	}
	free(worlds);
}

RyuWorld ryu_newWorld(void)
{
	RyuWorld worldHandle = CREATE_WORLD(UINT8_MAX, 0);

	if (stagedWorld == UINT8_MAX) {
		//no free worlds, allocate more
	}

	World *world = &worlds[stagedWorld];

	/* if the id of a world is pointing to its actual index,
	 it means that it's not free */
	assert(RYU_WORLD_INDEX(world->id) != stagedWorld &&
	       "attempting to use a non free world");

	uint8_t index = stagedWorld;
	stagedWorld = RYU_WORLD_INDEX(world->id);
	world->id = CREATE_WORLD(index,
				 RYU_WORLD_GENERATION(world->id)+1);
	worldHandle = world->id;

	/* allocate and setup entities */
	world->entities = malloc(RYU_INIT_ENTITY_COUNT * sizeof(EntityDesc));
	world->entityCount = RYU_INIT_ENTITY_COUNT;

	for (uint32_t i = 0; i < world->entityCount - 1; i++) {
		EntityDesc *desc = &world->entities[i];
		desc->id = CREATE_ENTITY(i + 1, 0, worldHandle);
	}
	world->entities[world->entityCount].id =
		CREATE_ENTITY(UINT32_MAX, 0, worldHandle);
	world->stagedEntity = 0;

	return worldHandle;
}

bool ryu_isWorldValid(RyuWorld handle)
{
	uint8_t index = RYU_WORLD_INDEX(handle);
	if (index > worldCount || index == UINT8_MAX) return false;
	World *world = &worlds[index];
	return world->id == handle;
}

void ryu_destroyWorld(RyuWorld handle)
{
	uint8_t index = RYU_WORLD_INDEX(handle);
	if (index > worldCount || index == UINT8_MAX) return;
	World *world = &worlds[index];
	if (world->id != handle) return;

	/* TODO: destruct components, blah blah blah */
	free(world->entities);
	world->id = CREATE_WORLD(stagedWorld,
				 RYU_WORLD_GENERATION(world->id));
	stagedWorld = index;
}


Entity ryu_newEntity(RyuWorld worldHandle)
{
	Entity handle = CREATE_ENTITY(UINT32_MAX, 0, worldHandle);

	uint8_t worldIndex = RYU_WORLD_INDEX(worldHandle);
	if (worldIndex > worldCount || worldIndex == UINT8_MAX) return handle;
	World *world = &worlds[worldIndex];
	if (world->id != worldHandle) return handle;

	if (world->stagedEntity == UINT32_MAX) {
		//no free entities, allocate
	}

	EntityDesc *desc = &world->entities[world->stagedEntity];

	/* if the id of an entity desc is pointing to its actual index,
	 it means that it's not free */
	assert(RYU_ENTITY_INDEX(desc->id)!=world->stagedEntity &&
	       "attempting to use a non free world");
	uint32_t index = world->stagedEntity;
	world->stagedEntity = RYU_ENTITY_INDEX(desc->id);
	desc->id = CREATE_ENTITY(index,
				 RYU_ENTITY_GENERATION(desc->id)+1,
				 worldHandle);
	desc->pending = false;
	handle = desc->id;

	return handle;
}

bool ryu_isEntityValid(Entity handle)
{
	RyuWorld worldHandle = RYU_ENTITY_WORLD(handle);
	uint8_t worldIndex = RYU_WORLD_INDEX(worldHandle);
	if (worldIndex > worldCount || worldIndex == UINT8_MAX) return false;
	World *world = &worlds[worldIndex];
	if (world->id != worldHandle) return false;

	uint32_t index = RYU_ENTITY_INDEX(handle);
	if (index > world->entityCount || index == UINT32_MAX) return false;
	EntityDesc *desc = &world->entities[index];
	return desc->id == handle;
}

void ryu_destroyEntity(Entity handle)
{
	RyuWorld worldHandle = RYU_ENTITY_WORLD(handle);
	uint8_t worldIndex = RYU_WORLD_INDEX(worldHandle);
	if (worldIndex > worldCount || worldIndex == UINT8_MAX) return;
	World *world = &worlds[worldIndex];
	if (world->id != worldHandle) return;

	uint32_t index = RYU_ENTITY_INDEX(handle);
	if (index > world->entityCount || index == UINT32_MAX) return;
	EntityDesc *desc = &world->entities[index];
	if (desc->id != handle) return;

	desc->pending = true;
}

bool ryu_isEntityPending(Entity handle)
{
	RyuWorld worldHandle = RYU_ENTITY_WORLD(handle);
	uint8_t worldIndex = RYU_WORLD_INDEX(worldHandle);
	if (worldIndex > worldCount || worldIndex == UINT8_MAX) return false;
	World *world = &worlds[worldIndex];
	if (world->id != worldHandle) return false;

	uint32_t index = RYU_ENTITY_INDEX(handle);
	if (index > world->entityCount || index == UINT32_MAX) return false;
	EntityDesc *desc = &world->entities[index];
	if (desc->id != handle) return false;

	return desc->pending;
}

//Notice: Be careful. Does not check validity.
static void destroyEntityImmediate(EntityDesc *desc, uint32_t entityIndex,
				   World *world)
{
	/* TODO: destruct components */
	desc->id = CREATE_ENTITY(world->stagedEntity,
				 RYU_ENTITY_GENERATION(desc->id),
				 world->id);
	desc->pending = false;
	world->stagedEntity = entityIndex;
}

void ryu_flush(RyuWorld worldHandle)
{
	uint8_t worldIndex = RYU_WORLD_INDEX(worldHandle);
	if (worldIndex > worldCount || worldIndex == UINT8_MAX) return;
	World *world = &worlds[worldIndex];
	if (world->id != worldHandle) return;

	for (uint32_t i = 0; i < world->entityCount; i++) {
		EntityDesc *desc = &world->entities[i];
		if (desc->pending) {
			// TODO: destruct components
			destroyEntityImmediate(desc, i, world);
		}
	}
}
