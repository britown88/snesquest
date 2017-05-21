#pragma once

#include "libutils/Defs.h"
#include "libutils/Rect.h"

typedef struct SNES_t SNES;
typedef struct TextureManager_t TextureManager;
typedef struct Texture_t Texture;
typedef struct FrameProfiler_t FrameProfiler;
typedef struct LogSpud_t LogSpud;
typedef struct DB_DBAssets DB_DBAssets;

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
   DB_DBAssets *db;

   const Window *window;
   Variables variables;
   Texture *snesTex;
   int testX, testY, testBGX, testBGY, testMosaic;
   int snesRenderWhite;
   boolean guiEnabled;
}AppData;
