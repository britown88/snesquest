#pragma once

#include "libutils/Defs.h"
#include "libutils/Math.h"
#include "libutils/Time.h"


typedef struct DeviceContext_t DeviceContext;
typedef struct Renderer_t Renderer;
typedef struct AppData_t AppData;

DeviceContext *deviceContextCreate();
void deviceContextDestroy(DeviceContext *self);

int deviceContextCreateWindow(DeviceContext *self, AppData *data);
void deviceContextPrepareForRendering(DeviceContext *self);
void deviceContextCommitRender(DeviceContext *self);
void deviceContextPollEvents(DeviceContext *self);

void deviceContextRenderGUI(DeviceContext *self, Renderer *r);
void deviceContextUpdateGUI(DeviceContext *self, AppData *data);

Int2 deviceContextGetWindowSize(DeviceContext *self);
Int2 deviceContextGetDrawableSize(DeviceContext *self);
Microseconds deviceContextGetTime(DeviceContext *self);
boolean deviceContextGetShouldClose(DeviceContext *self);

#define DC_FILE_ALL 1 //returns both directories and files
#define DC_FILE_DIR_ONLY 2  //returns only directories
#define DC_FILE_FILE_ONLY 3 //return only files

#include "libutils/StandardVectors.h"
int deviceContextListFiles(const char *root, int type, vec(StringPtr) **out, const char *ext);

