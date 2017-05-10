#pragma once

#include <malloc.h>
#include "libutils\extern_c.h"
#include "libutils\DLLBullshit.h"

SEXTERN_C

void* checkedMallocImpl(size_t sz, char *func, char* file, size_t line);
void* checkedCallocImpl(size_t count, size_t sz, char *func, char* file, size_t line);
void* uncheckedMallocImpl(size_t sz, char *func, char* file, size_t line);
void* uncheckedCallocImpl(size_t count, size_t sz, char* file, size_t line);
void checkedFreeImpl(void* mem);
void printMemoryLeaks();

END_SEXTERN_C
 
#ifdef _DEBUG
#define checkedMalloc(sz) checkedMallocImpl(sz, __FUNCTION__, __FILE__, __LINE__)
#define checkedCalloc(count, sz) checkedCallocImpl(count, sz, __FUNCTION__, __FILE__, __LINE__)
#define checkedFree(sz) checkedFreeImpl(sz)
#else
#define checkedMalloc(sz) malloc(sz)
#define checkedCalloc(count, sz) calloc(count, sz)
#define checkedFree(sz) free(sz)
#endif


 
