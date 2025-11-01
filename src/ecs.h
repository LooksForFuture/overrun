#ifndef __ECS_MAIN__
#define __ECS_MAIN__

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#define ECS_ENTITY_INDEX_MASK 0xFFFFFFFF00000000
#define ECS_ENTITY_GENERATION_MASK 0x00000000FFFFFFFF

#define ECS_ENTITY_INDEX(ent) ((ent >> 32) & 0xFFFFFFFF)
#define ECS_ENTITY_GENERATION(ent) (ent & ECS_ENTITY_GENERATION_MASK)

#define ECS_ID(token) ECS__##token
#define ECS_DECL_COMP(name) EcsComponent ECS_ID(name) = 0
#define ECS_REG_COMP(name) \
ECS_ID(name) = ecs_registerComponent(sizeof(name), alignof(name))

#define ECS_ARGS_1(a) ECS_ID(a)
#define ECS_ARGS_2(a,b) ECS_ID(a), ECS_ID(b)
#define ECS_ARGS_3(a,b,c) ECS_ID(a), ECS_ID(b), ECS_ID(c)
#define ECS_ARGS_4(a,b,c,d) ECS_ID(a), ECS_ID(b), ECS_ID(c), ECS_ID(d)
#define ECS_ARGS_5(a,b,c,d,e) ECS_ID(a), ECS_ID(b), ECS_ID(c), ECS_ID(d), ECS_ID(e)
#define ECS_ARGS_6(a,b,c,d,e,f) ECS_ID(a), ECS_ID(b), ECS_ID(c), ECS_ID(d), ECS_ID(e), ECS_ID(f)
#define ECS_ARGS_7(a,b,c,d,e,f,g) ECS_ID(a), ECS_ID(b), ECS_ID(c), ECS_ID(d), ECS_ID(e), ECS_ID(f), ECS_ID(g)
#define ECS_ARGS_8(a,b,c,d,e,f,g,h) ECS_ID(a), ECS_ID(b), ECS_ID(c), ECS_ID(d), ECS_ID(e), ECS_ID(f), ECS_ID(g), ECS_ID(h)
#define ECS_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME
#define ECS_ARGS(...) ECS_GET_MACRO(__VA_ARGS__, ECS_ARGS_8, ECS_ARGS_7, ECS_ARGS_6, ECS_ARGS_5, ECS_ARGS_4, ECS_ARGS_3, ECS_ARGS_2, ECS_ARGS_1)(__VA_ARGS__)

#define ECS_REG_ARCH(...) \
ecs_registerArchetype((EcsComponent[]){ECS_ARGS(__VA_ARGS__)}, \
	sizeof((EcsComponent[]){ECS_ARGS(__VA_ARGS__)}) / sizeof(EcsComponent))

typedef uint64_t Entity;
typedef uint32_t EcsComponent;

struct Archetype;
typedef struct Archetype Archetype;

void ecs_init(void);
void ecs_shutdown(void);

EcsComponent ecs_registerComponent(size_t, size_t);

Archetype *ecs_registerArchetype(EcsComponent *, size_t);

Entity ecs_newEntity(Archetype *arch);

#endif //__ECS_MAIN__
