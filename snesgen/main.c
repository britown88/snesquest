#include "Parser.h"

#include "libutils/BitBuffer.h"
#include "libutils/CheckedMemory.h"

#include "xxhash.h"
#include <stdio.h>

static void _processFileSTRING_ID(const char *fname, const char *fOut) {
   long fSize = 0;

   byte *buff = readFullFile(fname, &fSize);
   Tokenizer *tokens = tokenizerCreate((StringStream) { .pos = buff, .last = buff + fSize });
   tokenizerAcceptFile(tokens);

   checkedFree(buff);

   TokenStream tkstrm = { .pos = vecBegin(Token)(tokens->tokens),.last = vecEnd(Token)(tokens->tokens) };
   TokenStream *strm = &tkstrm;

   String *str = stringCreate("boolean");
   String *out = stringCreate("");

   while (!strmAtEnd(strm)) {
      Token *strToken = NULL;
      int rw = 0;

      // validates "S_ID("literal", 0)" and sets rw to a rewind count for if it fails

      if (strmAcceptIdentifierRaw(strm, "S_ID")) {++rw;
         while (strmAcceptSkippable(strm)) { ++rw; }
         if (strmAcceptOperator(strm, '(')) { ++rw;
         while (strmAcceptSkippable(strm)) { ++rw; }
            if (strToken = strmAcceptValueStringLiteral(strm)) { ++rw;
            while (strmAcceptSkippable(strm)) { ++rw; }
               if (strmAcceptOperator(strm, ',')) { ++rw;
               while (strmAcceptSkippable(strm)) { ++rw; }
                  if (strmAcceptValueInteger(strm)) { ++rw;
                  while (strmAcceptSkippable(strm)) { ++rw; }
                     if (strmAcceptOperator(strm, ')')) {
                        char buff[256] = { 0 };

                        sprintf(buff, "S_ID(\"%s\", %u)", c_str(strToken->str), XXH32(c_str(strToken->str), stringLen(strToken->str), 0));
                        stringConcat(out, buff);
                        continue;
                     }
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
   stringDestroy(str);
   tokenizerDestroy(tokens);
}

int main() {
   _processFileSTRING_ID("Parser.c", "Parser_t.c");
   printMemoryLeaks();
}