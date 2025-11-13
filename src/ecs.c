#include <ecs.h>

#include <zoner/zon_arena.h>
#include <zoner/zon_flpool.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTITY_COUNT 1024
#define MAX_COMPONENT_COUNT 1024
#define MAX_ARCHETYPE_COUNT 128
#define MAX_ARCH_ENTITY 256

#define CREATE_ENTITY(i, g) (((uint64_t)(i) << 32) | (g))

typedef struct {
	size_t size;
	size_t alignment;
} ComponentDesc;

struct Archetype {
	EcsComponent componentIds[8];
	void *storage[8];
	int compCount;
	Entity entities[MAX_ARCH_ENTITY];
	ZonFLPool indexPool; //for recording free indices
	int entCount;
};

typedef struct {
	Entity id;
	Archetype *arch;
	int index; //index in archetype
} EntityDesc;

// where we store ECS data and free on shutdown
static ZonArena ecsStorage;

// number of alive entities
static int entCount = 0;
static EntityDesc entDescs[MAX_ENTITY_COUNT];
static uint32_t nextFreeEntity = 0;

static int compCount = 0;
static ComponentDesc compDescs[MAX_COMPONENT_COUNT];

static int archCount = 0;
static Archetype archetypes[MAX_ARCHETYPE_COUNT];

Archetype *emptyArch = NULL; //empty arch for empty entities

void ecs_init(void)
{
	for (uint32_t i = 0; i < MAX_ENTITY_COUNT - 1; i++) {
		entDescs[i].id = CREATE_ENTITY(i+1, 0);
	}
	entDescs[MAX_ENTITY_COUNT-1].id = CREATE_ENTITY(UINT32_MAX, 0);

	nextFreeEntity = 0;
	entCount = 0;

	ecsStorage = zon_arenaCreate(malloc(65536), 65536);
	emptyArch = ecs_registerArchetype(NULL, 0);
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

static int comparComponent(const void *a, const void *b)
{
	return (*(EcsComponent *)a - *(EcsComponent *)b);
}

Archetype *ecs_registerArchetype(EcsComponent *components, size_t count)
{
	//sort components for faster iteration
	qsort(components, count, sizeof(EcsComponent), comparComponent);

	Archetype *arch = &archetypes[archCount++];
	for (size_t i = 0; i < count; i++) {
		const ComponentDesc desc = compDescs[components[i]];
		arch->componentIds[i] = components[i];
		arch->storage[i] =
			zon_arenaMalloc(&ecsStorage,
					MAX_ARCH_ENTITY * desc.size);
	}
	arch->compCount = count;
	arch->entCount = 0;

	arch->indexPool = zon_flpoolCreate(arch->entities,
					   sizeof(Entity), MAX_ARCH_ENTITY);

	return arch;
}

bool ecs_isValid(Entity ent)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	uint32_t gen = ECS_ENTITY_GENERATION(ent);
	return ECS_ENTITY_GENERATION(entDescs[index].id) == gen;
}

Entity ecs_newEntity(void) {
	return ecs_newEntityInArch(emptyArch);
}

Entity ecs_newEntityInArch(Archetype *arch)
{
	uint32_t currentFree = nextFreeEntity;
	EntityDesc *desc = &entDescs[currentFree];
	nextFreeEntity = ECS_ENTITY_INDEX(desc->id);

	uint32_t generation = ECS_ENTITY_GENERATION(desc->id);
	desc->id = CREATE_ENTITY(currentFree, generation + 1);
	entCount++;

	Entity *slot = zon_flpoolPop(&arch->indexPool);
	int index = (int)(slot - arch->entities);
	arch->entities[index] = desc->id;
	desc->arch = arch;
	desc->index = index;
	arch->entCount++;

	return desc->id;
}

void ecs_destroyEntity(Entity ent)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	uint32_t gen = ECS_ENTITY_GENERATION(ent);
	EntityDesc *desc = &entDescs[index];
	if (ECS_ENTITY_GENERATION(desc->id) != gen) return;

	Archetype *arch = desc->arch;
	zon_flpoolPush(&arch->indexPool, &arch->entities[desc->index]);
	arch->entCount--;
	desc->arch = NULL;

	desc->id = CREATE_ENTITY(nextFreeEntity, gen + 1);
	nextFreeEntity = index;
}

Archetype *ecs_getEntityArch(Entity ent) {
	return entDescs[ECS_ENTITY_INDEX(ent)].arch;
}

void *ecs_addComponent(Entity ent, EcsComponent comp)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	if (entDescs[index].id != ent) return NULL;

	Archetype *oldArch = entDescs[index].arch;
	int oldCount = oldArch->compCount;

	// check if the component is already present
	for (int i = 0; i < oldCount; i++) {
		if (oldArch->componentIds[i] == comp) {
			size_t size = compDescs[comp].size;
			return (uint8_t*)oldArch->storage[i] +
			       entDescs[index].index + size;
		}
	}

	int newCount = oldCount + 1;
	size_t marker = zon_arenaMarker(&ecsStorage);
	EcsComponent *ids = zon_arenaMalloc(&ecsStorage, newCount * sizeof(EcsComponent));

	int insertPos = 0;
	while (insertPos < oldCount &&
	       oldArch->componentIds[insertPos] < comp) insertPos++;

	//copy before insertPos
	if (insertPos > 0)
		memcpy(ids, oldArch->componentIds,
		       insertPos * sizeof(EcsComponent));
	ids[insertPos] = comp;
	if (oldCount - insertPos > 0)
		memcpy(ids + insertPos + 1,
		       oldArch->componentIds + insertPos,
		       (oldCount - insertPos) * sizeof(EcsComponent));

	//find or create archetype
	Archetype *newArch = NULL;
	for (int i = 0; i < archCount; i++) {
		Archetype *arch = &archetypes[i];
		if (arch->compCount == newCount &&
		    memcmp(arch->componentIds, ids,
			   newCount * sizeof(EcsComponent)) == 0) {
			newArch = arch;
			break;
		}
	}
	if (!newArch)
		newArch = ecs_registerArchetype(ids, newCount);

	//copy data from old to new archetype
	int oldIndex = entDescs[index].index;
	Entity *slot = zon_flpoolPop(&newArch->indexPool);
	int newIndex = (int)(slot - newArch->entities);
	newArch->entCount++;

	//--- before insertPos
	for (int i = 0; i < insertPos; i++) {
		size_t size = compDescs[oldArch->componentIds[i]].size;
		memcpy((uint8_t*)newArch->storage[i] + newIndex * size,
		       (uint8_t*)oldArch->storage[i] + oldIndex * size,
		       size);
	}

	//--- after insertPos
	for (int i = insertPos; i < oldCount; i++) {
		size_t size = compDescs[oldArch->componentIds[i]].size;
		memcpy((uint8_t*)newArch->storage[i+1] + newIndex * size,
		       (uint8_t*)oldArch->storage[i] + oldIndex * size,
		       size);
	}

	//init new component in new archetype
	size_t newSize = compDescs[comp].size;
	uint8_t* newCompPtr = (uint8_t*)newArch->storage[insertPos] + newIndex * newSize;
	memset(newCompPtr, 0, newSize);

	//remove entity index from old archetype
	zon_flpoolPush(&oldArch->indexPool, &oldArch->entities[oldIndex]);
	oldArch->entCount--;

	//update entity metadata
	newArch->entities[newIndex] = ent;
	entDescs[index].arch = newArch;
	entDescs[index].index = newIndex;

	zon_arenaRewind(&ecsStorage, marker);
	return (void*)newCompPtr;
}
