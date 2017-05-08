#pragma once

typedef struct GUI_t GUI;
typedef union SDL_Event SDL_Event;
typedef struct Renderer_t Renderer;

GUI *guiCreate();
void guiDestroy(GUI *self);

void guiBeginInput(GUI *self);
void guiEndInput(GUI *self);
int guiProcessInputEvent(GUI *self, SDL_Event *evt);

void guiInit(GUI *self);
void guiRender(GUI *self, Renderer *r);

int nuklear_overview(struct nk_context *ctx);

