#include "App.h"
#include "DeviceContext.h"
#include "Renderer.h"
#include "Config.h"

#include "shared/CheckedMemory.h"
#include "libutils/Defs.h"
#include "libutils/IncludeWindows.h"

#include <time.h>

const static Int2 nativeRes = { 512, 336 };

typedef struct {
   Matrix view;
} UBOMain;

struct App_t {
   boolean running;
   Microseconds lastUpdated;
   Renderer *renderer;
   DeviceContext *context;   


   //text stuff
   TextureManager *textureManager;
   Texture *tex;
   Shader *s;
   UBO *ubo;
   FBO *fbo;
   Model *testModel;

};

App *appCreate(Renderer *renderer, DeviceContext *context) {
   App *out = checkedCalloc(1, sizeof(App));

   out->renderer = renderer;
   out->context = context;


   out->textureManager = textureManagerCreate(NULL);
   out->s = shaderCreate("assets/shaders.glsl", ShaderParams_DiffuseTexture);
   out->ubo = uboCreate(sizeof(UBOMain));

   out->fbo = fboCreate(nativeRes, RepeatType_Clamp, FilterType_Nearest);

   TextureRequest request = {
      .repeatType = RepeatType_Clamp,
      .filterType = FilterType_Nearest,
      .path = stringIntern("assets/test.png")
   };

   out->tex = textureManagerGetTexture(out->textureManager, request);

   FVF_Pos2_Tex2_Col4 vertices[] = {
      { .pos2 = { 0.0f, 0.0f },.tex2 = { 0.0f, 0.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 1.0f, 0.0f },.tex2 = { 1.0f, 0.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 1.0f, 1.0f },.tex2 = { 1.0f, 1.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 0.0f, 0.0f },.tex2 = { 0.0f, 0.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 1.0f, 1.0f },.tex2 = { 1.0f, 1.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
      { .pos2 = { 0.0f, 1.0f },.tex2 = { 0.0f, 1.0f },.col4 = { 1.0f, 1.0f, 1.0f, 1.0f } },
   };

   out->testModel = FVF_Pos2_Tex2_Col4_CreateModel(vertices, 6, ModelStreamType_Static);

   return out;
}
void appDestroy(App *self) {
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

   r_bindUBO(self->renderer, self->ubo, 0);

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



static void _renderStep(App *self) {
   Renderer *r = self->renderer;

   StringView uModel = stringIntern("uModelMatrix");
   StringView uColor = stringIntern("uColorTransform");
   StringView uTexture = stringIntern("uTexMatrix");
   StringView uTextureSlot = stringIntern("uTexture");

   
   const Recti nativeViewport = { 0, 0, nativeRes.x, nativeRes.y };
   Int2 winSize = r_getSize(r);


   r_bindFBOToWrite(r, self->fbo);
   r_viewport(r, &nativeViewport);
   r_clear(r, &Black);

   r_enableAlphaBlending(r, true);

   UBOMain ubo = { 0 };
   matrixIdentity(&ubo.view);
   matrixOrtho(&ubo.view, 0.0f, (float)nativeViewport.right, (float)nativeViewport.bottom, 0.0f, 1.0f, -1.0f);
   r_setUBOData(r, self->ubo, ubo);

   r_setShader(r, self->s);

   Matrix model = { 0 };
   matrixIdentity(&model);
   matrixScale(&model, (Float3) { 100.0f, 100.0f, 1.0f });
   r_setMatrix(r, uModel, &model);
   r_setColor(r, uColor, &White);

   Matrix texMatrix = { 0 };
   matrixIdentity(&texMatrix);
   r_setMatrix(r, uTexture, &texMatrix);

   r_bindTexture(r, self->textureManager, self->tex, 0);
   r_setTextureSlot(r, uTextureSlot, 0);

   r_renderModel(r, self->testModel, ModelRenderType_Triangles);
   r_enableAlphaBlending(r, false);


   r_bindFBOToWrite(r, NULL);

   r_viewport(r, &(Recti){ 0, 0, winSize.x, winSize.y });
   r_clear(r, &Black);


   matrixIdentity(&ubo.view);
   matrixOrtho(&ubo.view, 0.0f, (float)winSize.x, 0.0, (float)winSize.y, 1.0f, -1.0f);
   r_setUBOData(r, self->ubo, ubo);

   r_setShader(r, self->s);

   matrixIdentity(&model);
   matrixScale(&model, (Float3) { (float)winSize.x, (float)winSize.y, 1.0f });
   r_setMatrix(r, uModel, &model);
   r_setColor(r, uColor, &White);

   matrixIdentity(&texMatrix);
   r_setMatrix(r, uTexture, &texMatrix);

   r_bindFBOToRender(r, self->fbo, 0);
   r_setTextureSlot(r, uTextureSlot, 0);

   r_renderModel(r, self->testModel, ModelRenderType_Triangles);


   r_finish(r);
   r_flush(r);
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