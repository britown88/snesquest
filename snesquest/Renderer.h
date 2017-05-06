#pragma once

typedef struct Renderer_t Renderer;
typedef struct DeviceContext_t DeviceContext;

Renderer *rendererCreate(DeviceContext *context);
void rendererDestroy(Renderer *self);

void rendererRender(Renderer *self);
