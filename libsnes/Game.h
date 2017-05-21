#pragma once

typedef struct Game_t Game;
typedef struct AppData_t AppData;

Game *gameCreate(AppData *data);
void gameDestroy(Game *self);

void gameStart(Game *self, AppData *data);
void gameUpdate(Game *self, AppData *data);
