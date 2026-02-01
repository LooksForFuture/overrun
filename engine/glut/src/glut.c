#include <glut/glut.h>

#include <stddef.h>
#include <stdlib.h>

void glut_init()
{}

void glut_shutdown()
{}

void *glut_malloc(size_t size)
{
	return malloc(size);
}

void *glut_realloc(void *ptr, size_t newSize)
{
	return realloc(ptr, newSize);
}

void glut_free(void *ptr)
{
	free(ptr);
}
