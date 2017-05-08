#pragma once

#include "libutils/Defs.h"
#include "libutils/Rect.h"

typedef struct SNES_t SNES;

typedef struct {
   Int2 nativeResolution;
   Recti nativeResolutionRect;
}Constants;

typedef struct {
   EMPTY_STRUCT;
}Variables;

typedef struct AppData_t {
   SNES *snes;

   const Constants constants;
   Variables variables;
   uint32_t snesTexHandle, logoTexHandle;
}AppData;
