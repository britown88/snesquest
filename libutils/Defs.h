#pragma once

#include <stdint.h>




#define SIGN(expr) ((expr==0)?0:((expr>0)?1:-1))

typedef uint8_t byte;
typedef uint16_t byte2;

typedef byte boolean;
#ifndef __cplusplus
#define false 0
#define true 1
#endif

#define INF ((size_t)-1)

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define EMPTY_STRUCT int UNUSED

#define SEGASSERT(...) if(!(__VA_ARGS__)){ int a = *(int*)0; }
/*
typedef struct {
   EMPTY_STRUCT;
} foo;
*/

