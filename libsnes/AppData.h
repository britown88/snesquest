#pragma once

#include "libutils/Defs.h"
#include "libutils/Rect.h"

typedef struct SNES_t SNES;
typedef struct TextureManager_t TextureManager;
typedef struct Texture_t Texture;
typedef struct FrameProfiler_t FrameProfiler;

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
   FrameProfiler *frameProfiler;

   const Constants constants;
   Variables variables;
   Texture *snesTex;
   int testX, testY;
   boolean snesRenderWhite;
}AppData;
