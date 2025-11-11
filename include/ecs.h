#ifndef __ECS_MAIN__
#define __ECS_MAIN__

#include <map.h>

#include <stdalign.h>
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

typedef uint64_t Entity;
typedef uint32_t EcsComponent;

struct Archetype;
typedef struct Archetype Archetype;

struct EcsQuery;
typedef struct EcsQuery EcsQuery;

/* initialize ecs */
void ecs_init(void);

/* free up resources used */
void ecs_shutdown(void);

/* register a component (returns an id) */
EcsComponent ecs_registerComponent(size_t, size_t);

/* create or find an archetyoe with the specified components */
Archetype *ecs_registerArchetype(EcsComponent *, size_t);

/* create an empty entity */
Entity ecs_newEntity(void);

/* create an entity in the specified archetype */
Entity ecs_newEntityInArch(Archetype *);

/* returns the archetype which the entity resides in */
Archetype *ecs_getEntityArch(Entity);

/* add a component to an entity (changes archetype) */
void *ecs_addComponent(Entity, EcsComponent);

#endif //__ECS_MAIN__
