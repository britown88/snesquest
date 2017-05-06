#ifdef _WIN32
#include "libutils/IncludeWindows.h"
#include <GL/GL.h>
#else
#include <gl.h>
#endif

#include "Renderer.h"
#include "DeviceContext.h"

#include "shared/CheckedMemory.h"



struct Renderer_t {
   DeviceContext *context;
};

Renderer *rendererCreate(DeviceContext *context) {
   Renderer *out = checkedCalloc(1, sizeof(Renderer));

   out->context = context;

   return out;
}

void rendererDestroy(Renderer *self) {
   checkedFree(self);
}

void rendererRender(Renderer *self) {
   glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   deviceContextCommitRender(self->context);
}