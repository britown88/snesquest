#include "DeviceContext.h"
#include "Config.h"
#include "GUI.h"

#include "shared/CheckedMemory.h"
#include "libutils/IncludeWindows.h"
#include "libutils/Defs.h"

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "AppData.h"


struct DeviceContext_t {
   SDL_Window *window;
   SDL_GLContext sdlContext;

   LARGE_INTEGER clock_freq, clock_start;
   Int2 size;
   boolean shouldClose;
   GUI *gui;
};

DeviceContext *deviceContextCreate() {
   DeviceContext *out = checkedCalloc(1, sizeof(DeviceContext));
   out->gui = guiCreate();
   return out;
}
void deviceContextDestroy(DeviceContext *self) {

   guiDestroy(self->gui);

   // Delete our OpengL context
   SDL_GL_DeleteContext(self->sdlContext);

   // Destroy our window
   SDL_DestroyWindow(self->window);

   // Shutdown SDL 2
   SDL_Quit();

   checkedFree(self);
}

int deviceContextCreateWindow(DeviceContext *self) {
   self->size = (Int2) { CONFIG_WINDOW_X, CONFIG_WINDOW_Y };

   SDL_Init(SDL_INIT_VIDEO);

   uint32_t flags = SDL_WINDOW_OPENGL;
   if (CONFIG_WINDOW_FULLSCREEN) {
      flags |= SDL_WINDOW_FULLSCREEN;
   }

   self->window = SDL_CreateWindow(CONFIG_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      self->size.x, self->size.y, SDL_WINDOW_OPENGL);

   if (!self->window) {
      return 1;
   }

   self->sdlContext = SDL_GL_CreateContext(self->window);

   if (!self->sdlContext) {
      return 1;
   }

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

   SDL_GL_SetSwapInterval(CONFIG_WINDOW_VSYNC ? 1 : 0);

   timeBeginPeriod(1);
   QueryPerformanceFrequency(&self->clock_freq);
   QueryPerformanceCounter(&self->clock_start);

   return 0;
}

void deviceContextPrepareForRendering(DeviceContext *self) {
   //do contrext-related stuff here
   glewInit();
   guiInit(self->gui);
}

void deviceContextRenderGUI(DeviceContext *self, Renderer *r) {
   guiRender(self->gui, r);
}

void deviceContextUpdateGUI(DeviceContext *self, AppData *data) {
   guiUpdate(self->gui, data);
}

void deviceContextCommitRender(DeviceContext *self) {
   SDL_GL_SwapWindow(self->window);
}
void deviceContextPollEvents(DeviceContext *self) {
   SDL_Event event;
   guiBeginInput(self->gui);
   while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
         self->shouldClose = true;
      }
      guiProcessInputEvent(self->gui, &event);
   }
   guiEndInput(self->gui);
}

Int2 deviceContextGetWindowSize(DeviceContext *self) {
   return self->size;
}
Microseconds deviceContextGetTime(DeviceContext *self) {
   LARGE_INTEGER end, elapsed;
   QueryPerformanceCounter(&end);

   elapsed.QuadPart = end.QuadPart - self->clock_start.QuadPart;
   elapsed.QuadPart *= 1000000;
   elapsed.QuadPart /= self->clock_freq.QuadPart;

   return elapsed.QuadPart;
}
boolean deviceContextGetShouldClose(DeviceContext *self) {
   return self->shouldClose;
}