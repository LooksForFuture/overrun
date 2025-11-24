#include <stdio.h>
#include <string.h>
#include "ecs.h"   // your header

//-------------------------------------------------------------
// Define test components
//-------------------------------------------------------------
typedef struct { float x, y; } C_Position;
typedef struct { float vx, vy; } C_Velocity;
typedef struct { int hp;      } C_Health;

// Component handles (will be set at runtime)
EcsComponent COMP_POS, COMP_VEL, COMP_HP;

//-------------------------------------------------------------
// Utilities
//-------------------------------------------------------------
void print_entity(Entity e)
{
    printf("Entity(%u|gen=%u)\n",
            ECS_ENTITY_INDEX(e),
            ECS_ENTITY_GENERATION(e));
}

void print_position(Entity e)
{
    C_Position* p = ecs_getComponent(e, COMP_POS);
    if (p) printf("   Pos(%.2f, %.2f)\n", p->x, p->y);
    else   printf("   No Position\n");
}

void print_velocity(Entity e)
{
    C_Velocity* v = ecs_getComponent(e, COMP_VEL);
    if (v) printf("   Velocity(%.2f, %.2f)\n", v->vx, v->vy);
    else   printf("   No Velocity\n");
}

void print_health(Entity e)
{
    C_Health* h = ecs_getComponent(e, COMP_HP);
    if (h) printf("   HP(%d)\n", h->hp);
    else   printf("   No Health\n");
}

void debug_entity(Entity e)
{
    printf("\n=== Debug Entity ===\n");
    print_entity(e);
    print_position(e);
    print_velocity(e);
    print_health(e);
}

//-------------------------------------------------------------
// main
//-------------------------------------------------------------
int main(void)
{
	ecs_init();
    //---------------------------------------------------------
    // Register components
    //---------------------------------------------------------
    COMP_POS = ecs_registerComponent(sizeof(C_Position),  alignof(C_Position));
    COMP_VEL = ecs_registerComponent(sizeof(C_Velocity),  alignof(C_Velocity));
    COMP_HP  = ecs_registerComponent(sizeof(C_Health),    alignof(C_Health));

    //---------------------------------------------------------
    // Create some initial archetypes manually
    // Archetype []  (empty)
    // Archetype [Position]
    // Archetype [Position, Velocity]
    //---------------------------------------------------------
    EcsComponent tmp1[] = { COMP_POS };
    ecs_registerArchetype(tmp1, 1);

    EcsComponent tmp2[] = { COMP_POS, COMP_VEL };
    ecs_registerArchetype(tmp2, 2);

    //---------------------------------------------------------
    // Create a query: want Position only
    //---------------------------------------------------------
    EcsQuery* Q_Pos;
    {
            EcsQueryDesc qd = { .include = (EcsComponent[]){COMP_POS}, .includeCount = 1 };
        Q_Pos = ecs_makeQuery(qd);
    }

    //---------------------------------------------------------
    // Create a query: want Position + Velocity (movement system)
    //---------------------------------------------------------
    EcsQuery* Q_Move;
    {
        EcsQueryDesc qd = {
		.include = (EcsComponent[]){COMP_POS, COMP_VEL},
            .includeCount = 2
        };
        Q_Move = ecs_makeQuery(qd);
    }

    //---------------------------------------------------------
    // Create entities
    //---------------------------------------------------------
    printf("\n=== Creating Entities ===\n");

    Entity e1 = ecs_newEntity();              // starts empty
    Entity e2 = ecs_newEntity();              // starts empty
    Entity e3 = ecs_newEntity();              // starts empty
    debug_entity(e1);

    //---------------------------------------------------------
    // Add components (IMMEDIATE MODE)
    //---------------------------------------------------------
    printf("\n=== Immediate Component Add ===\n");

    C_Position* p1 = ecs_addComponent(e1, COMP_POS);
    p1->x = 4; p1->y = 5;

    C_Velocity* v1 = ecs_addComponent(e1, COMP_VEL);
    v1->vx = 1; v1->vy = -1;

    C_Health* h1 = ecs_addComponent(e1, COMP_HP);
    h1->hp = 10;

    debug_entity(e1);

    //---------------------------------------------------------
    // DEFERRED MODE TEST
    // Deferred adds happen later
    //---------------------------------------------------------
    printf("\n=== Deferred Component Add ===\n");

    ecs_deferBegin();

    ecs_addComponent(e2, COMP_POS);
    ecs_addComponent(e2, COMP_VEL);

    C_Position* p3 = ecs_addComponent(e3, COMP_POS);
    p3->x = 100; p3->y = 200;

    // Try removing something too
    ecs_removeComponent(e1, COMP_HP);

    // Try destroying e3
    ecs_destroy(e3);

    ecs_deferEnd();

    debug_entity(e2);
    debug_entity(e1);

    if (ecs_isValid(e3)) {
        printf("ERROR: e3 should be destroyed\n");
    } else {
        printf("\nEntity e3 destroyed correctly!\n");
    }

    //---------------------------------------------------------
    // TEST QUERY ITERATION: Movement system
    //---------------------------------------------------------
    printf("\n=== Movement System Simulation ===\n");

    printf("Before movement:\n");
    EcsIter it = ecs_queryIter(Q_Move);
    while (ecs_iterNext(&it)) {
        C_Position* p = it.includes[0];
        C_Velocity* v = it.includes[1];
        printf("  Entity %d: Pos(%.2f,%.2f) Vel(%.2f,%.2f)\n",
            ECS_ENTITY_INDEX(it.entity), p->x, p->y, v->vx, v->vy);
    }

    printf("\n--- Step Simulation (pos += vel) ---\n");
    it = ecs_queryIter(Q_Move);
    while (ecs_iterNext(&it)) {
        C_Position* p = it.includes[0];
        C_Velocity* v = it.includes[1];
        p->x += v->vx;
        p->y += v->vy;
    }

    printf("\nAfter movement:\n");
    it = ecs_queryIter(Q_Move);
    while (ecs_iterNext(&it)) {
        C_Position* p = it.includes[0];
        C_Velocity* v = it.includes[1];
        printf("  Entity %d: Pos(%.2f,%.2f) Vel(%.2f,%.2f)\n",
            ECS_ENTITY_INDEX(it.entity), p->x, p->y, v->vx, v->vy);
    }

    printf("\n=== All Tests Completed ===\n");

    ecs_shutdown();
    return 0;
}
