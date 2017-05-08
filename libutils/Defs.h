#pragma once

#include <stdint.h>

#define SIGN(expr) ((expr==0)?0:((expr>0)?1:-1))

typedef uint8_t byte;
typedef uint16_t byte2;
typedef int16_t sbyte2;

typedef byte boolean;
#ifndef __cplusplus
#define false 0
#define true 1
#endif

#define INF ((size_t)-1)

#define UNUSED(a) (void)a
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a)/sizeof(a)[0])

#define EMPTY_STRUCT int UNUSED

#define SEGASSERT(...) if(!(__VA_ARGS__)){ int a = *(int*)0; }
/*
typedef struct {
   EMPTY_STRUCT;
} foo;
*/

#pragma pack(push, 1)

typedef struct {
   union {
      sbyte2 raw;
      struct {
         byte value; byte:7, sign : 1;
      } twos;
   };
} TwosComplement9;

typedef struct {
   union {
      sbyte2 raw;
      struct {
         byte2 integer : 12, :3, sign : 1;
      }twos;
   };
} TwosComplement13;

typedef struct {
   union {
      struct {
         byte2 fraction : 8, integer : 7, sign : 1;
      } fixedPoint;

      byte2 raw;
   };
} FixedPoint;

//colors
typedef struct {
   byte r, g, b;
} ColorRGB;

typedef struct {
   byte r, g, b, a;
} ColorRGBA;

typedef struct {
   float r, g, b;
} ColorRGBf;

typedef struct {
   float r, g, b, a;
} ColorRGBAf;

#pragma pack(pop)

extern const ColorRGBAf White;
extern const ColorRGBAf Gray;
extern const ColorRGBAf DkGray;
extern const ColorRGBAf LtGray;
extern const ColorRGBAf Black;
extern const ColorRGBAf Red;
extern const ColorRGBAf DkRed;
extern const ColorRGBAf Green;
extern const ColorRGBAf DkGreen;
extern const ColorRGBAf Blue;
extern const ColorRGBAf DkBlue;
extern const ColorRGBAf Cyan;
extern const ColorRGBAf Yellow;
extern const ColorRGBAf Magenta;

