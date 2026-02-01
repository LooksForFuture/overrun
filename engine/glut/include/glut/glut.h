#ifndef __GLUT_MAIN__
#define __GLUT_MAIN__

#include <stddef.h>

void glut_init();

void glut_shutdown();

void *glut_malloc(size_t);

void *glut_realloc(void *, size_t);

void glut_free(void *);

#endif //__GLUT_MAIN__
