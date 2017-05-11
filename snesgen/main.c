#include "Parser.h"

#include "libutils/BitBuffer.h"
#include "libutils/CheckedMemory.h"


#include "libsnes/DeviceContext.h"

#include "xxhash.h"
#include <stdio.h>

typedef struct {
   StringID key;
   String *val;
}SIDEntry;

static int _sidEntryCompare(SIDEntry *e1, SIDEntry *e2) { return e1->key == e2->key; }
static size_t _sidEntryHash(SIDEntry *entry) { return entry->key; }
static void _sidEntryDestroy(SIDEntry *entry) { stringDestroy(entry->val); }

#define HashTableT SIDEntry
#include "libutils\HashTable_Create.h"

typedef struct {
   ht(SIDEntry) *entries;
}SIDProcessing;


static void _processFileSTRING_ID(SIDProcessing *sid, const char *fname, const char *fOut) {
   long fSize = 0;

   printf("%s\n", fname);

   byte *buff = readFullFile(fname, &fSize);
   Tokenizer *tokens = tokenizerCreate((StringStream) { .pos = buff, .last = buff + fSize });
   tokenizerAcceptFile(tokens);

   checkedFree(buff);

   TokenStream tkstrm = { .pos = vecBegin(Token)(tokens->tokens),.last = vecEnd(Token)(tokens->tokens) };
   TokenStream *strm = &tkstrm;

   String *out = stringCreate("");

   while (!strmAtEnd(strm)) {
      Token *strToken = NULL;
      int rw = 0;

      // validates "S_ID("literal", 0)" and sets rw to a rewind count for if it fails
      char *funcName = "";
      boolean hit = false;
      if (strmAcceptIdentifierRaw(strm, "S_ID")) {
         funcName = "S_ID";
         hit = true;
      }
      else if (strmAcceptIdentifierRaw(strm, "S_HASH")) {
         funcName = "S_HASH";
         hit = true;
      }

         
      if(hit){
         ++rw;
         while (strmAcceptSkippable(strm)) { ++rw; }
         if (strmAcceptOperator(strm, '(')) { ++rw;
            while (strmAcceptSkippable(strm)) { ++rw; }
            if (strToken = strmAcceptValueStringLiteral(strm)) { ++rw;
               while (strmAcceptSkippable(strm)) { ++rw; }
               if (strmAcceptOperator(strm, ',')) { ++rw;
                  while (strmAcceptSkippable(strm)) { ++rw; }
                  strmNext(strm); ++rw;
                  while (strmAcceptSkippable(strm)) { ++rw; }
                  if (strmAcceptOperator(strm, ')')) {
                     char buff[256] = { 0 };
                     SIDEntry newEntry = { .key = XXH32(c_str(strToken->str), stringLen(strToken->str), 0) };

                     SIDEntry *found = htFind(SIDEntry)(sid->entries, &newEntry);
                     if (found) {
                        if (!stringEqual(strToken->str, found->val)) {
                           //hash collision found
                           sprintf(buff, "%s(\"%s\", HASH_COLLISION_WITH(\"%s\"))", funcName, c_str(strToken->str), c_str(found->val));
                           printf("HASH COLLISION FOUND: %s\n", c_str(strToken->str));
                        }
                        else {
                           sprintf(buff, "%s(\"%s\", %u)", funcName, c_str(strToken->str), found->key);
                        }
                     }
                     else {
                        //add new item
                        newEntry.val = stringCopy(strToken->str);
                        htInsert(SIDEntry)(sid->entries, &newEntry);
                        sprintf(buff, "%s(\"%s\", %u)", funcName, c_str(strToken->str), newEntry.key);
                     }
                     
                     stringConcat(out, buff);
                     continue;
                  }
               }
            }
         }
      }


      strmRewind(strm, rw);
      stringConcat(out, c_str(strmPeek(strm)->raw));
      strmNext(strm);
   }

   FILE *fout = fopen(fOut, "wb");
   fputs(c_str(out), fout);
   fclose(fout);

   stringDestroy(out);
   tokenizerDestroy(tokens);
}

static void _processDirectorySTRING_ID(SIDProcessing *sid, const char *dir) {
   vec(StringPtr) *dirs = 0, *headers = 0, *sources = 0;
   deviceContextListFiles(dir, DC_FILE_DIR_ONLY, &dirs, NULL);
   deviceContextListFiles(dir, DC_FILE_FILE_ONLY, &sources, "c");
   deviceContextListFiles(dir, DC_FILE_FILE_ONLY, &headers, "h");

   vecForEach(StringPtr, str, dirs, { _processDirectorySTRING_ID(sid, c_str(*str)); });
   vecForEach(StringPtr, str, headers, { _processFileSTRING_ID(sid, c_str(*str), c_str(*str));  });
   vecForEach(StringPtr, str, sources, { _processFileSTRING_ID(sid, c_str(*str), c_str(*str)); });

   if (dirs) { vecDestroy(StringPtr)(dirs); }
   if (headers) { vecDestroy(StringPtr)(headers); }
   if (sources) { vecDestroy(StringPtr)(sources); }
}



static void _runSID(const char *dir) {
   SIDProcessing sid;
   sid.entries = htCreate(SIDEntry)(&_sidEntryCompare, &_sidEntryHash, &_sidEntryDestroy);
   
   printf("snesgen: processing S_ID() instances in %s...\n", dir);
   _processDirectorySTRING_ID(&sid, dir);

   htDestroy(SIDEntry)(sid.entries);
}

int main(int argc, char *argv[]) {
   if (argc <= 1) {
      printf("snesgen: no option.\n");
      return;
   }

   //_runSID(argv[1]);

   printMemoryLeaks();
}