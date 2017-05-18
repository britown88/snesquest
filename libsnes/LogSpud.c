#include "LogSpud.h"
#include "AppData.h"

#include "libutils/CheckedMemory.h"

#define VectorTPart LogSpudEntry
#include "libutils/Vector_Impl.h"

struct LogSpud_t {
   AppData *data;
   vec(LogSpudEntry) *log;
};

static void _entryDestroy(LogSpudEntry *self) {
   stringDestroy(self->msg);
}


LogSpud *logSpudCreate(AppData *data) {
   LogSpud *out = checkedCalloc(1, sizeof(LogSpud));
   out->data = data;
   out->log = vecCreate(LogSpudEntry)(&_entryDestroy);
   return out;
}
void logSpudDestroy(LogSpud *self) {
   vecDestroy(LogSpudEntry)(self->log);
   checkedFree(self);
}


void logSpudPushRaw(LogSpud *self, const char *tag, SpudLevel level, const char *msg) {
   LogSpudEntry entry = { 0 };
   entry.level = level;
   entry.tag = tag;
   entry.msg = stringCreate(msg);
   vecPushBack(LogSpudEntry)(self->log, &entry);

}
void logSpudPush(LogSpud *self, const char *tag, SpudLevel level, String *msg) {
   LogSpudEntry entry = { 0 };
   entry.level = level;
   entry.tag = tag;
   entry.msg = stringCopy(msg);
   vecPushBack(LogSpudEntry)(self->log, &entry);

}
vec(LogSpudEntry) *logSpudGet(LogSpud *self) {
   return self->log;
}
