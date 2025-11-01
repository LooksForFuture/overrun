#include "ecs.h"

#include <zoner/zon_arena.h>

#include <stdint.h>
#include <stdlib.h>

#define MAX_ENTITY_COUNT 1024
#define MAX_COMPONENT_COUNT 1024
#define MAX_ARCHETYPE_COUNT 128
#define MAX_ENTITY_ARCH 256

#define CREATE_ENTITY(i, g) (((uint64_t)(i) << 32) | (g))

typedef struct {
	size_t size;
	size_t alignment;
} ComponentDesc;

struct Archetype {
	EcsComponent componentIds[8];
	void *componentStorage[8];
	int componentCount;
	Entity entities[MAX_ENTITY_ARCH];
	int entCount;
};

typedef struct {
	Entity id;
	Archetype *arch;
	int index; //index in archetype
} EntityDesc;

// where we store ECS data and free on shutdown
ZonArena ecsStorage;

// number of alive entities
int entCount = 0;
EntityDesc entDescs[MAX_ENTITY_COUNT];

int compCount = 0;
ComponentDesc compDescs[MAX_COMPONENT_COUNT];

int archCount = 0;
Archetype archetypes[MAX_ARCHETYPE_COUNT];

void ecs_init(void)
{
	for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
		entDescs[i].id = CREATE_ENTITY(i, 0);
	}
	ecsStorage = zon_arenaCreate(malloc(4096), 4096);
}

void ecs_shutdown(void)
{
	free(zon_arenaUnlock(&ecsStorage));
}

EcsComponent ecs_registerComponent(size_t size, size_t alignment)
{
	ComponentDesc *desc = &compDescs[compCount];
	desc->size = size;
	desc->alignment = alignment;
	return compCount++;
}

Archetype *ecs_registerArchetype(EcsComponent *components, size_t count)
{
	Archetype *archetype = &archetypes[archCount++];
	for (size_t i = 0; i < count; i++) {
		const ComponentDesc desc = compDescs[components[i]];
		archetype->componentIds[i] = components[i];
		archetype->componentStorage[i] =
			zon_arenaMalloc(&ecsStorage,
					MAX_ENTITY_ARCH * desc.size);
	}
	archetype->componentCount = count;
	archetype->entCount = 0;

	return archetype;
}

Entity ecs_newEntity(Archetype *arch)
{
	EntityDesc *desc = &entDescs[entCount];
	uint32_t index = ECS_ENTITY_INDEX(desc->id);
	uint32_t generation = ECS_ENTITY_GENERATION(desc->id);
	desc->id = CREATE_ENTITY(index, generation + 1);
	entCount++;

	arch->entities[arch->entCount] = desc->id;
	desc->arch = arch;
	desc->index = arch->entCount;
	arch->entCount++;

	return desc->id;
}
