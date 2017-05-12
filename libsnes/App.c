#include "App.h"
#include "DeviceContext.h"
#include "Renderer.h"
#include "Config.h"
#include "libsnes/snes.h"
#include "AppData.h"

#include "libutils/CheckedMemory.h"
#include "libutils/Defs.h"
#include "libutils/IncludeWindows.h"

#include <time.h>

#include "FrameProfiler.h"
#include "EncodedAssets.h"

static App *g_App;

typedef struct {
   Matrix view;
} UBOMain;

typedef struct {
   TextureManager *textureManager;
   
   Shader *baseShader;
   UBO *ubo;
   FBO *nativeFBO;
   Model *rectModel;

   StringView uModel, uColor, uTexture, uTextureSlot;

   Texture *snesTexture;
   ColorRGBA *snesBuffer;

   //testing
   Texture *logoImage;
}RenderData;

struct App_t {
   boolean running;
   Microseconds lastUpdated;
   Renderer *renderer;
   DeviceContext *context;

   Window winData;
   RenderData rData;
   SNES snes;
   AppData data;
   FrameProfiler frameProfiler;
};

static Window _buildWindowData() {
   return(Window){
      .windowResolution = { CONFIG_WINDOW_X , CONFIG_WINDOW_Y },
      .nativeResolution = { CONFIG_NATIVE_X , CONFIG_NATIVE_Y },
      .fullScreen = CONFIG_WINDOW_FULLSCREEN,
      .vsync = CONFIG_WINDOW_VSYNC,
      .targetFramerate = CONFIG_WINDOW_FRAMERATE
   };
}

static void _setupRenderData(App *app) {
   RenderData *self = &app->rData;
   self->textureManager = textureManagerCreate(NULL);
   self->baseShader = shaderCreateFromBuffer(enc_Shader, ShaderParams_DiffuseTexture|ShaderParams_Color);
   self->ubo = uboCreate(sizeof(UBOMain));

   self->nativeFBO = fboCreate(app->winData.nativeResolution, RepeatType_Clamp, FilterType_Nearest);

   TextureRequest request = {
      .repeatType = RepeatType_Clamp,
      .filterType = FilterType_Nearest,
      .path = stringIntern("assets/logo.png")
   };

   //self->logoImage = textureManagerGetTexture(self->textureManager, request);

   FVF_Pos2_Tex2_Col4 vertices[] = {
      { .pos2 = { 0.0f, 0.0f },.tex2 = { 0.0f, 0.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 1.0f, 0.0f },.tex2 = { 1.0f, 0.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 1.0f, 1.0f },.tex2 = { 1.0f, 1.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 0.0f, 0.0f },.tex2 = { 0.0f, 0.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 1.0f, 1.0f },.tex2 = { 1.0f, 1.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 0.0f, 1.0f },.tex2 = { 0.0f, 1.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
   };

   self->rectModel = FVF_Pos2_Tex2_Col4_CreateModel(vertices, 6, ModelStreamType_Static);

   self->snesTexture = textureCreateCustom(SNES_SCANLINE_WIDTH, SNES_SCANLINE_COUNT, RepeatType_Clamp, FilterType_Linear);
   self->snesBuffer = checkedCalloc(SNES_SCANLINE_WIDTH * SNES_SCANLINE_COUNT, sizeof(ColorRGBA));

   self->uModel = stringIntern("uModelMatrix");
   self->uColor = stringIntern("uColorTransform");
   self->uTexture = stringIntern("uTexMatrix");
   self->uTextureSlot = stringIntern("uTexture");
}

static void _renderDataDestroy(RenderData *self) {
   fboDestroy(self->nativeFBO);
   uboDestroy(self->ubo);
   shaderDestroy(self->baseShader);

   modelDestroy(self->rectModel);

   textureDestroy(self->snesTexture);
   checkedFree(self->snesBuffer);

   textureManagerDestroy(self->textureManager);
}



static void _setupTestSNES(SNES *snes) {
   int i = 0;

   SNESColor *pColor = &snes->cgram.objPalettes.palette16s[0].colors[1];
   pColor->r = 31;

   pColor = &snes->cgram.objPalettes.palette16s[0].colors[2];
   pColor->b = 31;

   pColor = &snes->cgram.objPalettes.palette16s[0].colors[3];
   pColor->g = 31;

   Char16 *testChar = (Char16*)&snes->vram;

   for (i = 0; i < 8; ++i) {
      testChar->tiles[0].rows[i].planes[i % 2] = 255;

      (testChar + 1)->tiles[0].rows[i].planes[i % 2] = 255;
      (testChar + 1)->tiles[0].rows[i].planes[1] = 255;

      (testChar + 16)->tiles[0].rows[i].planes[i % 2] = 255;
      (testChar + 16)->tiles[0].rows[i].planes[1] = 255;

      (testChar + 17)->tiles[0].rows[i].planes[!(i % 2)] = 255;
   }

   
   for (i = 0; i < SNES_SCANLINE_COUNT; ++i) {
      snes->hdma[i].objSizeAndBase.objSize = 5;
   }

   snes->oam.objCount = 128;

}

App *appCreate(Renderer *renderer, DeviceContext *context) {
   App *out = checkedCalloc(1, sizeof(App));
   g_App = out;

   out->renderer = renderer;
   out->context = context;

   out->winData = _buildWindowData();
   _setupRenderData(out); 


   _setupTestSNES(&out->snes);
   out->data.snes = &out->snes;
   out->data.snesTex = out->rData.snesTexture;

   out->data.textureManager = out->rData.textureManager;
   out->data.frameProfiler = &out->frameProfiler;

   (Window*)out->data.window = &out->winData;

   return out;
}
void appDestroy(App *self) {
   
   _renderDataDestroy(&self->rData);   

   checkedFree(self);
}

AppData *appGetData(App *self) { return &self->data; }
Microseconds appGetTime(App *self) { return deviceContextGetTime(self->context); }

App *appGet() {
   return g_App;
}

void appQuit(App *self) { self->running = false; }
int appRand(App *self, int lower, int upper) {
   return (rand() % (upper - lower)) + lower;
}

static void _start(App *self) {
   if(deviceContextCreateWindow(self->context, &self->data)) {
      return;
   }

   r_init(self->renderer);

   r_bindUBO(self->renderer, self->rData.ubo, 0);

   srand((unsigned int)time(NULL));

   self->running = true;
}

static void _renderBasicRectModel(App *self, Texture *tex, Float2 pos, Float2 size, ColorRGBAf color) {
   Renderer *r = self->renderer;

   Matrix model = { 0 };
   matrixIdentity(&model);
   matrixTranslate(&model, pos);
   matrixScale(&model, size);
   
   r_setMatrix(r, self->rData.uModel, &model);
   r_setColor(r, self->rData.uColor, &color);

   Matrix texMatrix = { 0 };
   matrixIdentity(&texMatrix);
   r_setMatrix(r, self->rData.uTexture, &texMatrix);

   r_bindTexture(r, tex, 0);
   r_setTextureSlot(r, self->rData.uTextureSlot, 0);

   r_renderModel(r, self->rData.rectModel, ModelRenderType_Triangles);
}

static void _gameStep(App *self) {

   frameProfilerStartEntry(&self->frameProfiler, PROFILE_GAME_UPDATE);

   int x = 0, y = 0;

   Int2 n = self->winData.nativeResolution;
   const Recti nativeViewport = { 0, 0, n.x, n.y };

   int xCount = 4;
   int yCount = 2;
   int spacing = 64;

   for (y = 0; y < yCount; ++y) {
      for (x = 0; x < xCount; ++x) {
         int idx = y * xCount + x;

         TwosComplement9 testX = { self->data.testX + x*spacing };
         if (testX.raw >= 256) {
            testX.raw -= 512;
         }

         switch (idx % 4) {
         case 0:
            self->snes.oam.secondary[idx / 4].x9_0 = testX.twos.sign;
            self->snes.oam.secondary[idx / 4].sz_0 = 1;
            break;
         case 1:
            self->snes.oam.secondary[idx / 4].x9_1 = testX.twos.sign;
            self->snes.oam.secondary[idx / 4].sz_1 = 1;
            break;
         case 2:
            self->snes.oam.secondary[idx / 4].x9_2 = testX.twos.sign;
            self->snes.oam.secondary[idx / 4].sz_2 = 1;
            break;
         case 3:
            self->snes.oam.secondary[idx / 4].x9_3 = testX.twos.sign;
            self->snes.oam.secondary[idx / 4].sz_3 = 1;
            break;
         }
         //self->snes.oam.secondary[idx].x9_0 = testX.twos.sign;
         self->snes.oam.primary[idx].x = testX.twos.value;
         self->snes.oam.primary[idx].y = self->data.testY + y*spacing;

         if (x % 2) {
            self->snes.oam.primary[idx].flipX = 1;
         }

         if (y % 2) {
            self->snes.oam.primary[idx].flipY = 1;
         }
      }
   }

   self->snes.oam.objCount = xCount*yCount;

   frameProfilerEndEntry(&self->frameProfiler, PROFILE_GAME_UPDATE);
}

static void _snesSoftwareRender(App *self) {
   frameProfilerStartEntry(&self->frameProfiler, PROFILE_SNES_RENDER);
   Renderer *r = self->renderer;

   int renderFlags = 0;
   if (self->data.snesRenderWhite) {
      renderFlags |= SNES_RENDER_DEBUG_WHITE;
   }

   snesRender(&self->snes, self->rData.snesBuffer, renderFlags);
   textureSetPixels(self->rData.snesTexture, (byte*)self->rData.snesBuffer);
   frameProfilerEndEntry(&self->frameProfiler, PROFILE_SNES_RENDER);
}

static void _prepareForNativeRender(App *self) {
   Renderer *r = self->renderer;

   Int2 n = self->winData.nativeResolution;
   const Recti nativeViewport = { 0, 0, n.x, n.y };

   r_bindFBOToWrite(r, self->rData.nativeFBO);
   r_viewport(r, &nativeViewport);
   r_clear(r, &DkGray);
   r_enableAlphaBlending(r, true);

   UBOMain ubo = { 0 };
   matrixIdentity(&ubo.view);
   matrixOrtho(&ubo.view, 0.0f, (float)nativeViewport.w, (float)nativeViewport.h, 0.0f, 1.0f, -1.0f);
   r_setUBOData(r, self->rData.ubo, ubo);

   r_setShader(r, self->rData.baseShader);
}

static void _renderScreen(App *self) {
   Renderer *r = self->renderer;

   Int2 n = self->winData.nativeResolution;
   const Recti nativeViewport = { 0, 0, n.x, n.y };
   Int2 winSize = r_getSize(r);

   r_enableAlphaBlending(r, false);

   r_bindFBOToWrite(r, NULL);

   r_viewport(r, &(Recti){ 0, 0, winSize.x, winSize.y });
   r_clear(r, &Black);

   UBOMain ubo = { 0 };
   matrixIdentity(&ubo.view);
   matrixOrtho(&ubo.view, 0.0f, (float)winSize.x, 0.0, (float)winSize.y, 1.0f, -1.0f);
   r_setUBOData(r, self->rData.ubo, ubo);

   r_setShader(r, self->rData.baseShader);

   Matrix model = { 0 };
   matrixIdentity(&model);
   matrixScale(&model, (Float2) { (float)winSize.x, (float)winSize.y });
   r_setMatrix(r, self->rData.uModel, &model);
   r_setColor(r, self->rData.uColor, &White);

   Matrix texMatrix = { 0 };
   matrixIdentity(&texMatrix);
   r_setMatrix(r, self->rData.uTexture, &texMatrix);

   r_bindFBOToRender(r, self->rData.nativeFBO, 0);
   r_setTextureSlot(r, self->rData.uTextureSlot, 0);

   r_renderModel(r, self->rData.rectModel, ModelRenderType_Triangles);

   r_finish(r);
   r_flush(r);
}

static void _renderGUI(App *self) {
   frameProfilerStartEntry(&self->frameProfiler, PROFILE_GUI_UPDATE);
   
   Renderer *r = self->renderer;
   Matrix model = { 0 };
   matrixIdentity(&model);

   r_setMatrix(r, self->rData.uModel, &model);
   r_setColor(r, self->rData.uColor, &White);

   Matrix texMatrix = { 0 };
   matrixIdentity(&texMatrix);
   r_setMatrix(r, self->rData.uTexture, &texMatrix);

   r_setTextureSlot(r, self->rData.uTextureSlot, 0);
   
   deviceContextUpdateGUI(self->context, &self->data);
   deviceContextRenderGUI(self->context, r);

   frameProfilerEndEntry(&self->frameProfiler, PROFILE_GUI_UPDATE);
}

static void _renderStep(App *self) {
   frameProfilerStartEntry(&self->frameProfiler, PROFILE_RENDER);

   //all ogl calls after this will be drawn proportional to the native vp
   //_prepareForNativeRender(self);

   ////TODO have this only draw the snesbuffer if not in gui mode
   //_renderGUI(self);

   ////render native vp fbo to screen
   //_renderScreen(self);



   //test render because maybe screw the fbo??
   Renderer *r = self->renderer;

   Int2 winSize = self->winData.windowResolution;
   const Recti winVP = { 0, 0, winSize.x, winSize.y };

   r_viewport(r, &winVP);
   r_clear(r, &DkGray);
   r_enableAlphaBlending(r, true);

   UBOMain ubo = { 0 };
   matrixIdentity(&ubo.view);
   matrixOrtho(&ubo.view, 0.0f, (float)winVP.w, (float)winVP.h, 0.0f, 1.0f, -1.0f);
   r_setUBOData(r, self->rData.ubo, ubo);

   r_setShader(r, self->rData.baseShader);


   if (self->data.guiEnabled) {
      _renderGUI(self);
   }
   else {
      Float2 size = { 0 };

      size.x = winSize.x;
      size.y = (size.x * 9.0f) / 16.0f;

      _renderBasicRectModel(self, self->rData.snesTexture, (Float2) { 0.0f, 0.0f }, size, White);

   }

   

   r_finish(r);
   r_flush(r);


   frameProfilerEndEntry(&self->frameProfiler, PROFILE_RENDER);
}

static void __updateDeviceContext(App *self) {
   deviceContextPollEvents(self->context, &self->data);

   //update the winsize into the appdata view
   self->winData.windowResolution = deviceContextGetWindowSize(self->context);

   if (deviceContextGetShouldClose(self->context)) {
      self->running = false;
      return;
   }
}

static void _step(App *self) {
   frameProfilerStartEntry(&self->frameProfiler, PROFILE_UPDATE);
   
   __updateDeviceContext(self);

   //game step
   _gameStep(self);

   //draw to snesbuffer
   _snesSoftwareRender(self);

   //hardware render
   _renderStep(self);

   frameProfilerEndEntry(&self->frameProfiler, PROFILE_UPDATE);
}

static Microseconds _getFrameTime() {
   static Microseconds out;
   static boolean outSet = false;
   if (!outSet) { out = (Microseconds)((1.0 / CONFIG_WINDOW_FRAMERATE) * 1000000); }
   return out;
}

static void freeUpCPU(Microseconds timeOffset) {
   if (timeOffset > 1500) {
      Sleep((DWORD)((timeOffset - 500) / 1000));
   }
   else if (timeOffset > 500) {
      SwitchToThread();
   }
}

static void _stepWithTiming(App *self) {
   Microseconds usPerFrame = _getFrameTime();
   Microseconds time = appGetTime(self);
   Microseconds deltaTime = time - self->lastUpdated;

   frameProfilerSetEntry(&self->frameProfiler, PROFILE_FULL_FRAME, deltaTime);
   if (deltaTime >= usPerFrame) {
      self->lastUpdated = time;
      _step(self);
   }
   else {
      freeUpCPU(usPerFrame - deltaTime);
   }
   ++self->frameProfiler.frame;
}

void appRun(App *self) {
   _start(self);
   while (self->running) {
      _stepWithTiming(self);
   }
   return;
}