#include <ecs.h>

#include <zoner/zon_arena.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTITY_COUNT 1024
#define MAX_COMPONENT_COUNT 64
#define MAX_ARCHETYPE_COUNT 128
#define MAX_QUERY_COUNT 64
#define MAX_ARCH_ENTITY 256

#define CREATE_ENTITY(i, g) (((uint64_t)(i) << 32) | (g))

//a bitmask with each bit representing a component
typedef uint64_t EcsMask;

typedef struct {
	size_t size;
	size_t alignment;
} ComponentDesc;

struct Archetype {
	EcsComponent componentIds[8];
	void *storage[8];
	int compIndexCache[MAX_COMPONENT_COUNT];
	int compCount;
	EcsMask mask;
	Entity entities[MAX_ARCH_ENTITY];
	int entCount;
};

typedef struct {
	Entity id;
	Archetype *arch;
	int slot; //index in archetype
} EntityDesc;

struct EcsQuery {
	EcsMask include;
	EcsMask exclude;
	Archetype *matches[32];
	int matchCount;
	EcsComponent includeList[8];
	int includeCount;
};

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

static int queryCount = 0;
static EcsQuery queries[MAX_QUERY_COUNT];

Archetype *emptyArch = NULL; //empty arch for empty entities

void ecs_init(void)
{
	for (uint32_t i = 0; i < MAX_ENTITY_COUNT; i++) {
		entDescs[i].id = CREATE_ENTITY(i, 0);
	}

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
	arch->mask = 0;
	memset(arch->compIndexCache, -1, sizeof(arch->compIndexCache));
	for (size_t i = 0; i < count; i++) {
		EcsComponent comp = components[i];
		const ComponentDesc desc = compDescs[comp];
		arch->componentIds[i] = comp;
		arch->mask |= (1ULL << comp);
		arch->storage[i] =
			zon_arenaMalloc(&ecsStorage,
					MAX_ARCH_ENTITY * desc.size);
		arch->compIndexCache[comp] = i;
	}
	arch->compCount = count;
	arch->entCount = 0;

	for (int i = 0; i < queryCount; i++) {
		EcsQuery *q = &queries[i];
		if ((arch->mask & q->include) == q->include &&
		    (arch->mask & q->exclude) == 0) {
			q->matches[q->matchCount++] = arch;
		}
	}

	return arch;
}

bool ecs_isValid(Entity ent)
{
	return entDescs[ECS_ENTITY_INDEX(ent)].id == ent;
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

	int slot = arch->entCount;
	arch->entities[slot] = desc->id;
	desc->arch = arch;
	desc->slot = slot;
	arch->entCount++;

	return desc->id;
}

void ecs_destroyEntity(Entity ent)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	uint32_t gen = ECS_ENTITY_GENERATION(ent);
	EntityDesc *desc = &entDescs[index];
	if (desc->id != ent) return;

	Archetype *arch = desc->arch;
	int slot = desc->slot;
	int last = --arch->entCount;

	if (slot != last) {
		Entity lastEnt = arch->entities[last];
		arch->entities[slot] = lastEnt;

		for (int i = 0; i < arch->compCount; i++) {
			size_t size = compDescs[arch->componentIds[i]].size;
			memcpy((uint8_t*)arch->storage[i] + slot * size,
			       (uint8_t*)arch->storage[i] + last * size, size);
		}

		entDescs[ECS_ENTITY_INDEX(lastEnt)].slot = slot;
	}

	desc->arch = NULL;
	desc->id = CREATE_ENTITY(nextFreeEntity, gen+1);
	desc->slot = -1;
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
	EcsMask mask = (1ULL << comp);
	if ((mask & oldArch->mask) == mask) {
		for (int i = 0; i < oldCount; i++) {
			if (oldArch->componentIds[i] == comp) {
				size_t size = compDescs[comp].size;
				return (uint8_t*)oldArch->storage[i] +
				       entDescs[index].slot * size;
			}
		}
	}

	int newCount = oldCount + 1;
	mask = oldArch->mask | (1ULL << comp);

	int insertPos = 0;
	while (insertPos < oldCount &&
	       oldArch->componentIds[insertPos] < comp) insertPos++;

	//find or create archetype
	Archetype *newArch = NULL;
	for (int i = 0; i < archCount; i++) {
		Archetype *arch = &archetypes[i];
		if (arch->mask == mask) {
			newArch = arch;
			break;
		}
	}
	if (!newArch) {
		size_t marker = zon_arenaMarker(&ecsStorage);
		EcsComponent *ids = zon_arenaMalloc(&ecsStorage, newCount * sizeof(EcsComponent));
		//copy before insertPos
		if (insertPos > 0)
			memcpy(ids, oldArch->componentIds,
			       insertPos * sizeof(EcsComponent));
		ids[insertPos] = comp;
		if (oldCount - insertPos > 0)
			memcpy(ids + insertPos + 1,
			       oldArch->componentIds + insertPos,
			       (oldCount - insertPos) * sizeof(EcsComponent));
		newArch = ecs_registerArchetype(ids, newCount);
		zon_arenaRewind(&ecsStorage, marker);
	}

	//copy data from old to new archetype
	int oldSlot = entDescs[index].slot;
	int newSlot = newArch->entCount++;

	//--- before insertPos
	for (int i = 0; i < insertPos; i++) {
		size_t size = compDescs[oldArch->componentIds[i]].size;
		memcpy((uint8_t*)newArch->storage[i] + newSlot * size,
		       (uint8_t*)oldArch->storage[i] + oldSlot * size, size);
	}

	//--- after insertPos
	for (int i = insertPos; i < oldCount; i++) {
		size_t size = compDescs[oldArch->componentIds[i]].size;
		memcpy((uint8_t*)newArch->storage[i+1] + newSlot * size,
		       (uint8_t*)oldArch->storage[i] + oldSlot * size, size);
	}

	//init new component in new archetype
	size_t newSize = compDescs[comp].size;
	uint8_t* newCompPtr = (uint8_t*)newArch->storage[insertPos] + newSlot * newSize;
	memset(newCompPtr, 0, newSize);

	//remove entity index from old archetype
	int last = --oldArch->entCount;
	if (oldSlot != last) {
		Entity lastEnt = oldArch->entities[last];
		oldArch->entities[oldSlot] = lastEnt;

		for (int i = 0; i < oldArch->compCount; i++) {
			size_t sz = compDescs[oldArch->componentIds[i]].size;
			memcpy((uint8_t*)oldArch->storage[i] + oldSlot * sz,
			       (uint8_t*)oldArch->storage[i] + last * sz, sz);
		}

		entDescs[ECS_ENTITY_INDEX(lastEnt)].slot = oldSlot;
	}

	//update entity metadata
	newArch->entities[newSlot] = ent;
	entDescs[index].arch = newArch;
	entDescs[index].slot = newSlot;

	return (void*)newCompPtr;
}

void *ecs_getComponent(Entity ent, EcsComponent comp)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	if (entDescs[index].id != ent) return NULL;

	Archetype *arch = entDescs[index].arch;
	int slot = entDescs[index].slot;

	int cidx = arch->compIndexCache[comp];
	if (cidx < 0) return NULL; //component not in arch

	size_t size = compDescs[comp].size;
	return (uint8_t*)arch->storage[cidx] + slot * size;
}

EcsQuery *ecs_makeQuery(EcsQueryDesc desc)
{
	EcsQuery *q = &queries[queryCount++];
	q->include = 0;
	q->exclude = 0;
	q->matchCount = 0;
	q->includeCount = desc.includeCount;

	for (int i = 0; i < desc.includeCount; i++) {
		q->include |= (1ULL << desc.include[i]);
		q->includeList[i] = desc.include[i];
	}
	for (int i = 0; i < desc.excludeCount; i++)
		q->exclude |= (1ULL << desc.exclude[i]);

	for (int i = 0; i < archCount; i++) {
		Archetype *arch = &archetypes[i];
		if ((arch->mask & q->include) == q->include &&
		    (arch->mask & q->exclude) == 0) {
			q->matches[q->matchCount++] = arch;
		}
	}

	return q;
}

EcsIter ecs_queryIter(EcsQuery *q)
{
	EcsIter it = {
		.query = q,
		.archIndex = 0,
		.slot = -1,
	};

	return it;
}

bool ecs_iterNext(EcsIter *it)
{
	if (!it->query) return false;
	EcsQuery *q = it->query;

	while (it->archIndex < q->matchCount) {
		Archetype *arch = q->matches[it->archIndex];
		it->slot++;
		if (it->slot >= arch->entCount) {
			it->archIndex++;
			it->slot = -1;
			continue;
		}

		it->entity = arch->entities[it->slot];
		for (int i = 0; i < q->includeCount; i++) {
			EcsComponent comp = q->includeList[i];
			int cidx = arch->compIndexCache[comp];
			size_t size = compDescs[comp].size;
			it->includes[i] = (uint8_t*)arch->storage[cidx] +
					  it->slot * size;
		}

		return true;
	}

	return false;
}
