#pragma once

#include "libutils/String.h"

typedef struct LogSpud_t LogSpud;
typedef struct AppData_t AppData;

LogSpud *logSpudCreate(AppData *data);
void logSpudDestroy(LogSpud *self);

typedef enum{
   LOG_INFO,
   LOG_WARN,
   LOG_ERR
}SpudLevel;

void logSpudPushRaw(LogSpud *self, const char *tag, SpudLevel level, const char *msg);
void logSpudPush(LogSpud *self, const char *tag, SpudLevel level, String *msg);

#define LOG(TAG, LEVEL, MSG, ...) { \
   const char __msgBuff[256] = {0}; \
   sprintf(__msgBuff, MSG, __VA_ARGS__); \
   logSpudPushRaw(appGetData(appGet())->log, TAG, LEVEL, __msgBuff); \
}

typedef struct {
   const char *tag;
   SpudLevel level;
   String *msg;
}LogSpudEntry;

#define VectorTPart LogSpudEntry
#include "libutils/Vector_Decl.h"

vec(LogSpudEntry) *logSpudGet(LogSpud *self);
