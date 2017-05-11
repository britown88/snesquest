#include "DeviceContext.h"
#include "Config.h"
#include "GUI.h"

#include "libutils/CheckedMemory.h"
#include "libutils/IncludeWindows.h"
#include "libutils/Defs.h"

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include <strsafe.h>

#include "AppData.h"
#include "App.h"

struct DeviceContext_t {
   SDL_Window *window;
   SDL_GLContext sdlContext;

   LARGE_INTEGER clock_freq, clock_start;
   Int2 winSize, winDrawableSize;
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

int deviceContextCreateWindow(DeviceContext *self, AppData *data) {

   SetProcessDPIAware();

   self->winSize = data->window->windowResolution;

   SDL_Init(SDL_INIT_VIDEO);

   uint32_t flags = 0;
   if (data->window->fullScreen) {
      flags |= SDL_WINDOW_FULLSCREEN;
   }

   flags |= SDL_WINDOW_OPENGL;
   flags |= SDL_WINDOW_ALLOW_HIGHDPI;
   flags |= SDL_WINDOW_RESIZABLE;

   self->window = SDL_CreateWindow(CONFIG_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      self->winSize.x, self->winSize.y, flags);

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

   SDL_GL_SetSwapInterval(data->window->vsync ? 1 : 0);

   SDL_GetWindowSize(self->window, &self->winSize.x, &self->winSize.y);
   SDL_GL_GetDrawableSize(self->window, &self->winDrawableSize.x, &self->winDrawableSize.y);

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

static void _handleWindowEvent(DeviceContext *self, SDL_Event *event) {
   switch (event->window.event) {
   case SDL_WINDOWEVENT_SIZE_CHANGED:
      SDL_GetWindowSize(self->window, &self->winSize.x, &self->winSize.y);
      SDL_GL_GetDrawableSize(self->window, &self->winDrawableSize.x, &self->winDrawableSize.y);
      break;

   case SDL_WINDOWEVENT_MAXIMIZED:
      break;

   case SDL_WINDOWEVENT_MINIMIZED:
      break;

   case SDL_WINDOWEVENT_RESTORED:
      break;

   case SDL_WINDOWEVENT_CLOSE:
      break;
   }
}


void deviceContextPollEvents(DeviceContext *self, AppData *data) {
   SDL_Event event;

   boolean doGUI = data->guiEnabled;

   if (doGUI) { guiBeginInput(self->gui); }
   
   while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
         self->shouldClose = true;
         return;
      }

      switch (event.type) {
      case SDL_WINDOWEVENT:
         _handleWindowEvent(self, &event);
         break;
      case SDL_KEYUP:
         if (event.key.keysym.sym == SDLK_F1) {
            data->guiEnabled = !data->guiEnabled;
         }
         break;

      }

      if (doGUI) { guiProcessInputEvent(self->gui, &event); }
   }

   if (doGUI) { guiEndInput(self->gui); }
}

Int2 deviceContextGetWindowSize(DeviceContext *self) {
   return self->winSize;
}
Int2 deviceContextGetDrawableSize(DeviceContext *self) {
   return self->winDrawableSize;
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

int deviceContextListFiles(const char *root, int type, vec(StringPtr) **out, const char *ext) {
   WIN32_FIND_DATA ffd;
   TCHAR szDir[MAX_PATH];
   size_t length_of_arg;
   HANDLE hFind = INVALID_HANDLE_VALUE;
   DWORD dwError = 0;
   int extLen = 0;

   // Check that the input path plus 3 is not longer than MAX_PATH.
   // Three characters are for the "\*" plus NULL appended below.

   StringCchLength(root, MAX_PATH, &length_of_arg);

   if (length_of_arg > (MAX_PATH - 3)) {
      //_tprintf(TEXT("\nDirectory path is too long.\n"));
      return 1;
   }

   //_tprintf(TEXT("\nTarget directory is %s\n\n"), argv[1]);

   // Prepare string for use with FindFile functions.  First, copy the
   // string to a buffer, then append '\*' to the directory name.

   StringCchCopy(szDir, MAX_PATH, root);
   StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

   // Find the first file in the directory.
#ifdef SEGA_UWP
   //TODO: make this work with UWP
   hFind = INVALID_HANDLE_VALUE;
#else
   hFind = FindFirstFile(szDir, &ffd);
#endif



   if (INVALID_HANDLE_VALUE == hFind) {
      //DisplayErrorBox(TEXT("FindFirstFile"));
      return dwError;
   }

   *out = vecCreate(StringPtr)(&stringPtrDestroy);

   if (ext) {
      extLen = strlen(ext);
   }

   // List all the files in the directory with some info about them.
   do {

      if (type == DC_FILE_ALL ||
         (type == DC_FILE_DIR_ONLY && ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) ||
         (type == DC_FILE_FILE_ONLY && !(ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))) {

         if (!strcmp(ffd.cFileName, ".") || !strcmp(ffd.cFileName, "..")) {
            continue;
         }

         String *fname = stringCreate(root);
         stringConcat(fname, "/");
         stringConcat(fname, ffd.cFileName);

         if (ext) {
            int len = strlen(ffd.cFileName);
            size_t dotPos;
            char *str;

            if (len <= extLen + 1) {
               stringDestroy(fname);
               continue;
            }

            dotPos = stringFindLastOf(fname, ".");
            str = (char*)c_str(fname) + dotPos + 1;

            if (dotPos < stringNPos &&
               strlen(str) == extLen &&
               !memcmp(str, ext, extLen)) {

               vecPushBack(StringPtr)(*out, &fname);
            }
            else {
               stringDestroy(fname);
            }
         }
         else {
            vecPushBack(StringPtr)(*out, &fname);
         }
      }


   } while (FindNextFile(hFind, &ffd) != 0);


   FindClose(hFind);
   return 0;
}