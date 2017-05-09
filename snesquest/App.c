#include "App.h"
#include "DeviceContext.h"
#include "Renderer.h"
#include "Config.h"
#include "libsnes/snes.h"
#include "AppData.h"

#include "shared/CheckedMemory.h"
#include "libutils/Defs.h"
#include "libutils/IncludeWindows.h"

#include <time.h>


const static Int2 nativeRes = { 1280, 720 };


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

static void _setupRenderData(RenderData *self) {
   self->textureManager = textureManagerCreate(NULL);
   self->baseShader = shaderCreate("assets/shaders.glsl", ShaderParams_DiffuseTexture|ShaderParams_Color);
   self->ubo = uboCreate(sizeof(UBOMain));

   self->nativeFBO = fboCreate(nativeRes, RepeatType_Clamp, FilterType_Nearest);

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

   self->snesTexture = textureCreateCustom(512, 168, RepeatType_Clamp, FilterType_Nearest);
   self->snesBuffer = checkedCalloc(512 * 168, sizeof(ColorRGBA));

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

struct App_t {
   boolean running;
   Microseconds lastUpdated;
   Renderer *renderer;
   DeviceContext *context;   


   RenderData rData;
   SNES snes;
   AppData data;
};

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

   
   for (i = 0; i < 168; ++i) {
      snes->hdma[i].objSizeAndBase.objSize = 3;
   }

   snes->oam.objCount = 128;
}

App *appCreate(Renderer *renderer, DeviceContext *context) {
   App *out = checkedCalloc(1, sizeof(App));

   out->renderer = renderer;
   out->context = context;

   _setupRenderData(&out->rData); 

   _setupTestSNES(&out->snes);

   out->data.textureManager = out->rData.textureManager;

   return out;
}
void appDestroy(App *self) {
   
   _renderDataDestroy(&self->rData);   

   checkedFree(self);
}

Microseconds appGetTime(App *self) { return deviceContextGetTime(self->context); }
void appQuit(App *self) { self->running = false; }
int appRand(App *self, int lower, int upper) {
   return (rand() % (upper - lower)) + lower;
}

static void _start(App *self) {
   if(deviceContextCreateWindow(self->context)) {
      return;
   }

   r_init(self->renderer);

   r_bindUBO(self->renderer, self->rData.ubo, 0);

   srand((unsigned int)time(NULL));

   self->running = true;
}

static Microseconds _getFrameTime() {
   static Microseconds out;
   static boolean outSet = false;
   if (!outSet) {  out = (Microseconds)((1.0 / CONFIG_WINDOW_FRAMERATE) * 1000000); }
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

static int testCount = 10000, testCount2 = 0;
static void _renderNative(App *self) {
   Renderer *r = self->renderer;
   int x = 0, y = 0;

   const Recti nativeViewport = { 0, 0, nativeRes.x, nativeRes.y };

   for (y = 0; y < 5; ++y) {
      for (x = 0; x < 8; ++x) {
         int idx = y * 8 + x;



         TwosComplement9 testX = { self->data.testX + x*34 };
         if (testX.raw >= 256) {
            testX.raw -= 512;
         }

         switch (idx % 4) {
         case 0:
            self->snes.oam.secondary[idx / 4].x9_0 = testX.twos.sign;
            break;
         case 1:
            self->snes.oam.secondary[idx / 4].x9_1 = testX.twos.sign;
            break;
         case 2:
            self->snes.oam.secondary[idx / 4].x9_2 = testX.twos.sign;
            break;
         case 3:
            self->snes.oam.secondary[idx / 4].x9_3 = testX.twos.sign;
            break;
         }
         //self->snes.oam.secondary[idx].x9_0 = testX.twos.sign;
         self->snes.oam.primary[idx].x = testX.twos.value;
         self->snes.oam.primary[idx].y = self->data.testY + y*34;

         if (x % 2) {
            self->snes.oam.primary[idx].flipX = 1;
         }

         if (y % 2) {
            self->snes.oam.primary[idx].flipY = 1;
         }
      }
   }

   self->snes.oam.objCount = 5*8;


   

   //TwosComplement9 testX2 = { (sbyte2)(((testCount * 2) % (256 + 32)) - 32) };
   //self->snes.oam.secondary[0].x9_1 = testX2.twos.sign;
   //self->snes.oam.primary[1].x = testX2.twos.value;
   //self->snes.oam.primary[1].y = ((testCount * 2) % (168 + 32)) - 16;

   //self->snes.oam.primary[1].x = (testCount * 2) % 256;
   //self->snes.oam.primary[1].y = (testCount * 2) % 248;

   //for (j = 2; j < 128; ++j) {
   //   //self->snes.oam.primary[j].x = (testCount +j) % 256;
   //   //self->snes.oam.primary[j].y = (testCount +j*j) % 248;


   //   TwosComplement9 testX = { (sbyte2)(testCount + j % (256 + 32) - 32) };

   //   switch (j % 4) {
   //   case 0:
   //      self->snes.oam.secondary[j / 4].x9_0 = testX.twos.sign;
   //      break;
   //   case 1:
   //      self->snes.oam.secondary[j / 4].x9_1 = testX.twos.sign;
   //      break;
   //   case 2:
   //      self->snes.oam.secondary[j / 4].x9_2 = testX.twos.sign;
   //      break;
   //   case 3:
   //      self->snes.oam.secondary[j / 4].x9_3 = testX.twos.sign;
   //      break;
   //   }
   //   
   //   self->snes.oam.primary[j].x = testX.twos.value;
   //   self->snes.oam.primary[j].y = ((testCount + j) % (168 + 32)) - 16;

   //}

   
   ++testCount;

   self->data.snes = &self->snes;
   self->data.snesTexHandle = textureGetGLHandle(self->rData.snesTexture);
   

   deviceContextUpdateGUI(self->context, &self->data);

   int renderFlags = 0;

   if (self->data.snesRenderWhite) {
      renderFlags |= SNES_RENDER_DEBUG_WHITE;
   }

   snesRender(&self->snes, self->rData.snesBuffer, renderFlags);
   textureSetPixels(self->rData.snesTexture, (byte*)self->rData.snesBuffer);
   //_renderBasicRectModel(self, self->rData.snesTexture, (Float2) { 0.0f, 0.0f }, (Float2) { 1024.0f, 672.0f }, White);



   //test aramis
   //float aramisSize = 64.0f;
   //_renderBasicRectModel(self, self->rData.testImage, (Float2) { 0.0f, nativeRes.y - aramisSize }, (Float2) { aramisSize, aramisSize }, White);
}

static void _prepareForNativeRender(App *self) {
   Renderer *r = self->renderer;

   const Recti nativeViewport = { 0, 0, nativeRes.x, nativeRes.y };

   r_bindFBOToWrite(r, self->rData.nativeFBO);
   r_viewport(r, &nativeViewport);
   r_clear(r, &DkGray);
   r_enableAlphaBlending(r, true);

   UBOMain ubo = { 0 };
   matrixIdentity(&ubo.view);
   matrixOrtho(&ubo.view, 0.0f, (float)nativeViewport.right, (float)nativeViewport.bottom, 0.0f, 1.0f, -1.0f);
   r_setUBOData(r, self->rData.ubo, ubo);

   r_setShader(r, self->rData.baseShader);
}

static void _renderScreen(App *self) {
   Renderer *r = self->renderer;

   const Recti nativeViewport = { 0, 0, nativeRes.x, nativeRes.y };
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
   Renderer *r = self->renderer;
   Matrix model = { 0 };
   matrixIdentity(&model);

   r_setMatrix(r, self->rData.uModel, &model);
   r_setColor(r, self->rData.uColor, &White);

   Matrix texMatrix = { 0 };
   matrixIdentity(&texMatrix);
   r_setMatrix(r, self->rData.uTexture, &texMatrix);

   r_setTextureSlot(r, self->rData.uTextureSlot, 0);
   
   deviceContextRenderGUI(self->context, r);
}

static void _renderStep(App *self) {
   _prepareForNativeRender(self);
   _renderNative(self);
   _renderGUI(self);
   _renderScreen(self);
}

static void _singleUpdate(App *self) {
   deviceContextPollEvents(self->context);

   if (deviceContextGetShouldClose(self->context)) {
      self->running = false;
      return;
   }

   //game step

   //game render

   //gui shit   

   _renderStep(self);
}

static void _step(App *self) {
   Microseconds usPerFrame = _getFrameTime();
   Microseconds time = appGetTime(self);
   Microseconds deltaTime = time - self->lastUpdated;

   if (deltaTime >= usPerFrame) {
      self->lastUpdated = time;
      _singleUpdate(self);
   }
   else {
      freeUpCPU(usPerFrame - deltaTime);
   }
}

void appRun(App *self) {
   _start(self);
   while (self->running) {
      _step(self);
   }
   return;
}