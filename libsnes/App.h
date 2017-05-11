#pragma once

#include "libutils/Time.h"

typedef struct App_t App;
typedef struct DeviceContext_t DeviceContext;
typedef struct Renderer_t Renderer;
typedef struct AppData_t AppData;

App *appCreate(Renderer *renderer, DeviceContext *context);
void appDestroy(App *self);

void appRun(App *self);

App *appGet();
Microseconds appGetTime(App *app);

//AppData *appGetData(App *self);