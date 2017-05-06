#include "App.h"
#include "Renderer.h"
#include "DeviceContext.h"

#include "shared/CheckedMemory.h"

int main() {
   DeviceContext *context = deviceContextCreate();
   Renderer *renderer = rendererCreate(context);
   App *app = appCreate(renderer, context);

   appRun(app);

   rendererDestroy(renderer);
   deviceContextDestroy(context);
   appDestroy(app);

   printMemoryLeaks();
   return 0;
}