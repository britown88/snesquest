#include "App.h"
#include "DeviceContext.h"
#include "Renderer.h"
#include "Config.h"

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
   Texture *testImage;
}RenderData;

static void _setupRenderData(RenderData *self) {
   self->textureManager = textureManagerCreate(NULL);
   self->baseShader = shaderCreate("assets/shaders.glsl", ShaderParams_DiffuseTexture|ShaderParams_Color);
   self->ubo = uboCreate(sizeof(UBOMain));

   self->nativeFBO = fboCreate(nativeRes, RepeatType_Clamp, FilterType_Nearest);

   TextureRequest request = {
      .repeatType = RepeatType_Clamp,
      .filterType = FilterType_Nearest,
      .path = stringIntern("assets/test.png")
   };

   self->testImage = textureManagerGetTexture(self->textureManager, request);

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


   //text stuff
   RenderData rData;
};



App *appCreate(Renderer *renderer, DeviceContext *context) {
   App *out = checkedCalloc(1, sizeof(App));

   out->renderer = renderer;
   out->context = context;

   _setupRenderData(&out->rData);   

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

static int _testCounter = 0;
static void _renderNative(App *self) {
   Renderer *r = self->renderer;

   const Recti nativeViewport = { 0, 0, nativeRes.x, nativeRes.y };


   int x, y;
   for (y = 0; y < 168; ++y) {
      for (x = 0; x < 512; ++x) {
         if (_testCounter % 168 == y) {
            self->rData.snesBuffer[y * 512 + x] = (ColorRGBA) { 255, 0, 0, 255 };
         }
         else {
            byte gscale = rand() % 256;
            self->rData.snesBuffer[y * 512 + x] = (ColorRGBA) { gscale, gscale, gscale, 255 };
         }
      }      
   }
   ++_testCounter;
   textureSetPixels(self->rData.snesTexture, (byte*)self->rData.snesBuffer);
   _renderBasicRectModel(self, self->rData.snesTexture, (Float2) { 0.0f, 0.0f }, (Float2) { 1024.0f, 672.0f }, White);

   //test aramis
   float aramisSize = 64.0f;
   _renderBasicRectModel(self, self->rData.testImage, (Float2) { nativeRes.x - aramisSize, nativeRes.y - aramisSize }, (Float2) { aramisSize, aramisSize }, White);
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