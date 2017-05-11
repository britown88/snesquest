#include "libsnes/App.h"
#include "libsnes/Renderer.h"
#include "libsnes/DeviceContext.h"

#include "libutils/CheckedMemory.h"

int main() {
   DeviceContext *context = deviceContextCreate();
   Renderer *renderer = rendererCreate(context);
   App *app = appCreate(renderer, context);

   appRun(app);

   rendererDestroy(renderer);
   deviceContextDestroy(context);
   appDestroy(app);

   printMemoryLeaks();

   HashedString test = S_HASH("hmm", 1046785858);
   StringID id = S_ID("hmm", 1046785858);

   return 0;
}