#include <ecs.h>

#include <zoner/zon_arena.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
Currently there are some hard static limits to the ECS which may turn
dynamic in the next versions

Entities are stored in a static array and are treated as a free list pool
When an Entity is not alive, its id would be the index of the next free
entity. The last free entity would have UINT32_MAX as its index to
indicate that there is no more free entities

During the deferred mode, all the operations done on the components are
stored in an arena which would be then applied on the main storage. The
arena gets cleaned up at the end of deferred mode

An entity with NULL archetype record means it has been created in
deferred mode and has not been put in an archetype (not implemented yet)
these entities do not get processed by queries and systems until the
flush puts them in an archetype (invisiable to queries)
 */

/* static limits */
#define MAX_ENTITY_COUNT 1024
#define MAX_COMPONENT_COUNT 64
#define MAX_ARCHETYPE_COUNT 128
#define MAX_QUERY_COUNT 64
#define MAX_ARCH_ENTITY 256

#define CREATE_ENTITY(i, g) (((uint64_t)(i) << 32) | (g))

/*
  Due to the fact that there would be only 64 components at max,
  we can write our filters as bitmasks with each component id being
  a bit of the bitmask
 */

// component id to bitmask
#define COMP_BIT(c) (1ULL << (c))

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

typedef struct {
	bool active; //already in dirty list?
	bool destroy; //entity must die
	EcsMask addMask; //bit mask of components to add
	EcsMask remMask; //bit mask of components to remove
	void *data[MAX_COMPONENT_COUNT]; //pointer to staged comp data
} CmdBucket;

// where we store component data and free on shutdown
static ZonArena ecsStorage;
// for storing staged components data
static ZonArena cmdStorage;

static int entCount = 0; // number of alive entities
static EntityDesc entDescs[MAX_ENTITY_COUNT];
static uint32_t nextFreeEntity = 0;

static CmdBucket cmdBuckets[MAX_ENTITY_COUNT];
//entities to be processed at flush
static uint32_t dirties[MAX_ENTITY_COUNT];
static size_t dirtyCount = 0; //how many buckets to process at flush

static int compCount = 0;
static ComponentDesc compDescs[MAX_COMPONENT_COUNT];

static int archCount = 0;
static Archetype archetypes[MAX_ARCHETYPE_COUNT];

static int queryCount = 0;
static EcsQuery queries[MAX_QUERY_COUNT];

static bool inDeferred = false;

static Archetype *emptyArch = NULL; //empty arch for empty entities

void ecs_init(void)
{
	//mark all entities as free
	for (uint32_t i = 0; i < MAX_ENTITY_COUNT - 1; i++) {
		entDescs[i].id = CREATE_ENTITY(i + 1, 0);
		entDescs[i].arch = NULL;
		entDescs[i].slot = -1;
	}

	entDescs[MAX_ENTITY_COUNT - 1].id =
		CREATE_ENTITY(UINT32_MAX, 0);
	nextFreeEntity = 0;
	entCount = 0;
	dirtyCount = 0;

	ecsStorage = zon_arenaCreate(malloc(65536), 65536);
	cmdStorage = zon_arenaCreate(malloc(65536), 65536);
	emptyArch = ecs_registerArchetype(NULL, 0);
}

void ecs_shutdown(void)
{
	free(zon_arenaUnlock(&ecsStorage));
	free(zon_arenaUnlock(&cmdStorage));
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
    EcsComponent ca = *(const EcsComponent *)a;
    EcsComponent cb = *(const EcsComponent *)b;
    if (ca < cb) return -1;
    if (ca > cb) return 1;
    return 0;
}

Archetype *ecs_registerArchetype(EcsComponent *components, size_t count)
{
	//sort components for faster iteration
	if (count > 0 && components != NULL)
		qsort(components, count, sizeof(EcsComponent), comparComponent);

	Archetype *arch = &archetypes[archCount++];
	arch->mask = 0;

	//write every index cache to -1 to signify it's absence
	for (int i = 0; i < MAX_COMPONENT_COUNT; ++i)
		arch->compIndexCache[i] = -1;

	for (int i = 0; i < count; i++) {
		EcsComponent comp = components[i];
		const ComponentDesc desc = compDescs[comp];
		arch->componentIds[i] = comp;
		arch->mask |= COMP_BIT(comp);
		arch->storage[i] =
			zon_arenaAlloc(&ecsStorage,
				       MAX_ARCH_ENTITY * desc.size,
				       desc.alignment);
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

Archetype *ecs_getEntityArch(Entity ent)
{
	return entDescs[ECS_ENTITY_INDEX(ent)].arch;
}

Entity ecs_newEntity(void)
{
	return ecs_newEntityInArch(emptyArch);
}


Entity ecs_newEntityInArch(Archetype *arch)
{
	// Ensure the free-list head is valid
	if (nextFreeEntity == UINT32_MAX) {
		fprintf(stderr, "ecs_newEntityInArch: out of entity slots\n");
		exit(1);
	}
	
	uint32_t currentFree = nextFreeEntity;
	EntityDesc *desc = &entDescs[currentFree];
	nextFreeEntity = ECS_ENTITY_INDEX(desc->id);

	uint32_t generation = ECS_ENTITY_GENERATION(desc->id);
	desc->id = CREATE_ENTITY(currentFree, generation + 1);

	if (arch->entCount >= MAX_ARCH_ENTITY) {
		fprintf(stderr, "ecs_newEntityInArch: archetype full (max %d)\n", MAX_ARCH_ENTITY);
		exit(1);
	}
	entCount++;

	int slot = arch->entCount;
	arch->entities[slot] = desc->id;
	desc->arch = arch;
	desc->slot = slot;
	arch->entCount++;

	return desc->id;
}

static void markBucketDirty(uint32_t entIndex)
{
	CmdBucket *buck = &cmdBuckets[entIndex];
	if (!buck->active) {
		buck->active = true;
		dirties[dirtyCount++] = entIndex;
	}
}

static void ecs_destroyImmediate(Entity ent)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	uint32_t gen = ECS_ENTITY_GENERATION(ent);
	EntityDesc *desc = &entDescs[index];

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

	entCount--;
}

static void ecs_destroyDeferred(Entity ent)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	CmdBucket *buck = &cmdBuckets[index];

	//ignore if already schduled to destroy
	if (buck->destroy) return;

	buck->destroy = true;
	buck->addMask = 0;
	buck->remMask = 0;

	markBucketDirty(index);
}

void ecs_destroy(Entity ent)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	if (entDescs[index].id != ent) return;

	if (inDeferred) ecs_destroyDeferred(ent);
	else ecs_destroyImmediate(ent);
}

static void *ecs_addComponentImmediate(Entity ent, EcsComponent comp)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	Archetype *oldArch = entDescs[index].arch;
	int oldCount = oldArch->compCount;

	// check if the component is already present
	int insertPos = oldArch->compIndexCache[comp];
	if (insertPos >= 0) {
		size_t size = compDescs[comp].size;
		return (uint8_t*)oldArch->storage[insertPos] +
		       entDescs[index].slot * size;
	}

	insertPos = 0;
	while (insertPos < oldCount &&
	       oldArch->componentIds[insertPos] < comp) insertPos++;

	int newCount = oldCount + 1;
	EcsMask mask = oldArch->mask | COMP_BIT(comp);

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

static void *ecs_addComponentDeferred(Entity ent, EcsComponent comp)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	CmdBucket *buck = &cmdBuckets[index];

	// ignore if scheduled to be destroyed
	if (buck->destroy) return NULL;

	/* if currently has the component and not scheduled
	   to be removed, return it */
	Archetype *arch = entDescs[index].arch;
	int cidx = arch->compIndexCache[comp];
	if (cidx >= 0) {
		//has the component
		//check if not scheduled to be removed
		if ((buck->remMask & COMP_BIT(comp)) == 0) {
			size_t sz = compDescs[comp].size;
			return (uint8_t*)arch->storage[cidx] +
			       entDescs[index].slot * sz;
		}
	}

	//if ADD already staged, return pointer to storage
	if (buck->addMask & COMP_BIT(comp)) {
		return buck->data[comp];
	}

	//Not staged yet. Stage it.
	size_t sz = compDescs[comp].size;
	void *buf = zon_arenaMalloc(&cmdStorage, sz);
	if (!buf) return NULL;
	buck->data[comp] = buf;
	buck->addMask |= COMP_BIT(comp);
	buck->remMask &= ~COMP_BIT(comp);

	markBucketDirty(index);
	return buf;
}

void *ecs_addComponent(Entity ent, EcsComponent comp)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	if (entDescs[index].id != ent) return NULL;

	if (inDeferred) return ecs_addComponentDeferred(ent, comp);
	else return ecs_addComponentImmediate(ent, comp);
}

static void ecs_removeComponentImmediate(Entity ent, EcsComponent comp)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	Archetype *oldArch = entDescs[index].arch;
	int oldCount = oldArch->compCount;

	// check if the component is present
	int remPos = oldArch->compIndexCache[comp];
	if (remPos < 0) return;

	remPos = 0;
	while (remPos < oldCount &&
	       oldArch->componentIds[remPos] < comp) remPos++;

	int newCount = oldCount - 1;
	EcsMask mask = oldArch->mask & ~COMP_BIT(comp);

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
		//copy before remPos
		if (remPos > 0)
			memcpy(ids, oldArch->componentIds,
			       remPos * sizeof(EcsComponent));
		if (oldCount - remPos - 1 > 0)
			memcpy(ids + remPos,
			       oldArch->componentIds + remPos + 1,
			       (oldCount - remPos - 1) * sizeof(EcsComponent));
		newArch = ecs_registerArchetype(ids, newCount);
		zon_arenaRewind(&ecsStorage, marker);
	}

	//copy data from old to new archetype
	int oldSlot = entDescs[index].slot;
	int newSlot = newArch->entCount++;

	//--- before remPos
	for (int i = 0; i < remPos; i++) {
		size_t size = compDescs[oldArch->componentIds[i]].size;
		memcpy((uint8_t*)newArch->storage[i] + newSlot * size,
		       (uint8_t*)oldArch->storage[i] + oldSlot * size, size);
	}

	//--- after remPos
	for (int i = remPos + 1; i < oldCount; i++) {
		size_t size = compDescs[oldArch->componentIds[i]].size;
		memcpy((uint8_t*)newArch->storage[i-1] + newSlot * size,
		       (uint8_t*)oldArch->storage[i] + oldSlot * size, size);
	}

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
}

static void ecs_removeComponentDeferred(Entity ent, EcsComponent comp)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	CmdBucket *buck = &cmdBuckets[index];

	//ignore if entity is scheduled to be destroyed
	if (buck->destroy) return;

	//ignore if component is neither in the arch or staged
	Archetype *arch = entDescs[index].arch;
	if (arch->compIndexCache[comp] < 0 &&
	    !(buck->addMask & COMP_BIT(comp))) return;

	//mark removal and disable ADD command
	buck->remMask |= COMP_BIT(comp);
	buck->addMask &= ~COMP_BIT(comp);
	//disabling add does not delete staged component data

	markBucketDirty(index);
}

void ecs_removeComponent(Entity ent, EcsComponent comp)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	if (entDescs[index].id != ent) return;

	if (inDeferred) ecs_removeComponentDeferred(ent, comp);
	else ecs_removeComponentImmediate(ent, comp);
}

void *ecs_getComponent(Entity ent, EcsComponent comp)
{
	uint32_t index = ECS_ENTITY_INDEX(ent);
	if (entDescs[index].id != ent) return NULL;

	if (!inDeferred) {
		Archetype *arch = entDescs[index].arch;
		int slot = entDescs[index].slot;

		int cidx = arch->compIndexCache[comp];
		if (cidx < 0) return NULL; //component not in arch

		size_t size = compDescs[comp].size;
		return (uint8_t*)arch->storage[cidx] + slot * size;
	}

	CmdBucket *buck = &cmdBuckets[index];

	//ignore if schduled to destroy
	if (buck->destroy) return NULL;
	else if (buck->remMask & COMP_BIT(comp)) return NULL;
	else if (buck->addMask & COMP_BIT(comp))
		return buck->data[comp];

	//check inside the current archetype
	Archetype *arch = entDescs[index].arch;
	int slot = entDescs[index].slot;
	int cidx = arch->compIndexCache[comp];
	if (cidx < 0) return NULL;
	size_t size = compDescs[comp].size;
	return (uint8_t*)arch->storage[cidx] + slot * size;
}

static void flushCommands()
{
	if (!inDeferred) return;

	for (int i = 0; i < dirtyCount; i++) {
		uint32_t entIndex = dirties[i];
		CmdBucket *buck = &cmdBuckets[entIndex];

		//check if bucket is empty
		if (!buck->destroy &&
		    !buck->addMask &&
		    !buck->remMask) continue;

		//check entity validity
		Entity ent = entDescs[entIndex].id;
		if (ECS_ENTITY_INDEX(ent) != entIndex) continue;

		//check for destroy flag
		if (buck->destroy) {
			ecs_destroyImmediate(ent);
			continue;
		}

		uint64_t remMask = buck->remMask;
		while (remMask) {
			uint64_t bits = remMask;
			uint64_t comp = 0;

			while (!(bits & 01)) {
				++comp;
				bits>>=1;
			}
			remMask &= remMask - 1;

			ecs_removeComponentImmediate(ent, (EcsComponent)comp);
		}

		uint64_t addMask = buck->addMask;
		while (addMask) {
			uint64_t bits = addMask;
			uint64_t comp = 0;

			while (!(bits & 01)) {
				++comp;
				bits>>=1;
			}
			addMask &= addMask - 1;

			void *dst = ecs_addComponentImmediate(ent, (EcsComponent)comp);
			if (dst && buck->data[comp]) {
				size_t sz = compDescs[comp].size;
				memcpy(dst, buck->data[comp], sz);
			}
		}
	}
}

void ecs_deferBegin(void)
{
	if (inDeferred) return;

	dirtyCount = 0;
	memset(dirties, 0, sizeof(dirties));
	inDeferred = true;
}

void ecs_deferEnd(void)
{
	if (!inDeferred) return;
	flushCommands();
	zon_arenaRewind(&cmdStorage, 0);
	inDeferred = false;
}

EcsQuery *ecs_makeQuery(EcsQueryDesc desc)
{
	EcsQuery *q = &queries[queryCount++];
	q->include = 0;
	q->exclude = 0;
	q->matchCount = 0;
	q->includeCount = desc.includeCount;

	for (int i = 0; i < desc.includeCount; i++) {
		q->include |= COMP_BIT(desc.include[i]);
		q->includeList[i] = desc.include[i];
	}
	for (int i = 0; i < desc.excludeCount; i++)
		q->exclude |= COMP_BIT(desc.exclude[i]);

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
