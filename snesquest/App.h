#pragma once

typedef struct App_t App;
typedef struct DeviceContext_t DeviceContext;
typedef struct Renderer_t Renderer;

App *appCreate(Renderer *renderer, DeviceContext *context);
void appDestroy(App *self);

void appRun(App *self);