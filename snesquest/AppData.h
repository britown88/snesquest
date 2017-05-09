#pragma once

#include "libutils/Defs.h"
#include "libutils/Rect.h"

typedef struct SNES_t SNES;
typedef struct TextureManager_t TextureManager;

typedef struct {
   Int2 nativeResolution;
   Recti nativeResolutionRect;
}Constants;

typedef struct {
   EMPTY_STRUCT;
}Variables;

typedef struct AppData_t {
   SNES *snes;
   TextureManager *textureManager;

   const Constants constants;
   Variables variables;
   uint32_t snesTexHandle;
   int testX, testY;
   boolean snesRenderWhite;
}AppData;
