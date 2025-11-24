#ifndef __ECS_MAIN__
#define __ECS_MAIN__

#include <map.h>

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ECS_ENTITY_INDEX_MASK 0xFFFFFFFF00000000
#define ECS_ENTITY_GENERATION_MASK 0x00000000FFFFFFFF

#define ECS_ENTITY_INDEX(ent) (((ent) >> 32) & 0xFFFFFFFF)
#define ECS_ENTITY_GENERATION(ent) ((ent) & ECS_ENTITY_GENERATION_MASK)

#define ECS_ID(token) ECS__##token
#define ECS_DECL_COMP(name) EcsComponent ECS_ID(name) = 0
#define ECS_REG_COMP(name) \
ECS_ID(name) = ecs_registerComponent(sizeof(name), alignof(name))

#define ECS_REG_ARCH(...) \
ecs_registerArchetype((EcsComponent[]){MAP_LIST(ECS_ID, __VA_ARGS__)}, \
MAP_COUNT(__VA_ARGS__))

#define ECS_ADD_COMPONENT(entity, component) \
(component*)ecs_addComponent(entity, ECS_ID(component))

#define ECS_GET_COMPONENT(entity, component) \
(component*)ecs_getComponent(entity, ECS_ID(component))

#define ECS_QUERY(...) ecs_makeQuery((EcsQueryDesc){__VA_ARGS__})

#define ECS_ACCESS(type, ...) .type = (EcsComponent[]){ \
	MAP_LIST(ECS_ID, __VA_ARGS__) \
	}, .type##Count = MAP_COUNT(__VA_ARGS__)

typedef uint64_t Entity;
typedef uint64_t EcsComponent;

struct Archetype;
typedef struct Archetype Archetype;

struct EcsQuery;
typedef struct EcsQuery EcsQuery;

typedef struct {
	EcsComponent *include;
	int includeCount;
	EcsComponent *exclude;
	int excludeCount;
} EcsQueryDesc;

typedef struct {
	EcsQuery *query;
	Entity entity;
	int archIndex;
	int slot;
	void *includes[8];
} EcsIter;

/* initialize ecs */
void ecs_init(void);

/* free up resources used */
void ecs_shutdown(void);

/* enter deferred mode */
void ecs_deferBegin(void);

/* exit deferred mode */
void ecs_deferEnd(void);

/* register a component (returns an id) */
EcsComponent ecs_registerComponent(size_t, size_t);

/* create an archetyoe with the specified set of components */
Archetype *ecs_registerArchetype(EcsComponent *, size_t);

/* checks if an entity is invalid (still alive?) */
bool ecs_isValid(Entity);

/* create an empty entity */
Entity ecs_newEntity(void);

/* destroy the entity (makes it invalid) */
void ecs_destroy(Entity);

/* create an entity in the specified archetype */
Entity ecs_newEntityInArch(Archetype *);

/* returns the archetype which the entity resides in */
Archetype *ecs_getEntityArch(Entity);

/* add a component to an entity (changes archetype) */
void *ecs_addComponent(Entity, EcsComponent);

/* remove component from entity */
void ecs_removeComponent(Entity, EcsComponent);

/* get component if available */
void *ecs_getComponent(Entity, EcsComponent);

/* make a query based on the defined accesses */
EcsQuery *ecs_makeQuery(EcsQueryDesc);

/* make an iterator for getting entities from query */
EcsIter ecs_queryIter(EcsQuery *);

/* iterates over the query and checks if an entity is available */
bool ecs_iterNext(EcsIter *);

#endif //__ECS_MAIN__
