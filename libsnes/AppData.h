#pragma once

#include "libutils/Defs.h"
#include "libutils/Rect.h"

typedef struct SNES_t SNES;
typedef struct TextureManager_t TextureManager;
typedef struct Texture_t Texture;
typedef struct FrameProfiler_t FrameProfiler;
typedef struct LogSpud_t LogSpud;

typedef struct {
   Int2 windowResolution;
   
   Int2 nativeResolution;
   boolean fullScreen;
   boolean vsync;
   float targetFramerate;
}Window;

typedef struct {
   EMPTY_STRUCT;
}Variables;

typedef struct AppData_t {
   SNES *snes;
   TextureManager *textureManager;
   FrameProfiler *frameProfiler;
   LogSpud *log;

   const Window *window;
   Variables variables;
   Texture *snesTex;
   int testX, testY;
   int snesRenderWhite;
   boolean guiEnabled;
}AppData;
