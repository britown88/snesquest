#include "DeviceContext.h"
#include "Config.h"

#include "shared/CheckedMemory.h"
#include "libutils/IncludeWindows.h"
#include "libutils/Defs.h"

#include <SDL2/SDL.h>


struct DeviceContext_t {
   SDL_Window *window;
   SDL_GLContext sdlContext;

   LARGE_INTEGER clock_freq, clock_start;
   Int2 size;
   boolean shouldClose;
};

DeviceContext *deviceContextCreate() {
   DeviceContext *out = checkedCalloc(1, sizeof(DeviceContext));
   return out;
}
void deviceContextDestroy(DeviceContext *self) {
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

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

   SDL_GL_SetSwapInterval(CONFIG_WINDOW_VSYNC ? 1 : 0);

   timeBeginPeriod(1);
   QueryPerformanceFrequency(&self->clock_freq);
   QueryPerformanceCounter(&self->clock_start);

   return 0;
}

void deviceContextCommitRender(DeviceContext *self) {
   SDL_GL_SwapWindow(self->window);
}
void deviceContextPollEvents(DeviceContext *self) {
   SDL_Event event;
   while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
         self->shouldClose = true;
      }
   }
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