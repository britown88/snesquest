#include "libutils/BitBuffer.h"
#include "libutils/CheckedMemory.h"

#include <stdio.h>

#include "Tools.h"
#include "Parser.h"

static const char *EncodeIdentifier = "ENCODE_ASSET";

static Token *_acceptAsset(TokenStream *strm) {
   Token *startPos = strmPeek(strm);

   if (strmAcceptIdentifierRaw(strm, EncodeIdentifier)) {
      while (strmAcceptSkippable(strm));

      if (!strmAcceptOperator(strm, '(')) {
         strm->pos = startPos;
         return NULL;
      }

      while (strmAcceptSkippable(strm));

      Token *pToken = strmAcceptValueStringLiteral(strm);
      if (!pToken || !pToken->str) {
         strm->pos = startPos;
         return NULL;
      }

      while (strmAcceptSkippable(strm));

      if (!strmAcceptOperator(strm, ',')) {
         strm->pos = startPos;
         return NULL;
      }

      while (strmAcceptSkippable(strm));

      while (true) {
         if (strmAtEnd(strm)) {
            strm->pos = startPos;
            return NULL;
         }

         if (strmAcceptOperator(strm, ')')) {
            //made it to the end
            return pToken;
         }
         else {
            strmNext(strm);
         }
      }
   }

   return NULL;
}


void runAssetGen(const char *file) {
   long fSize = 0;

   printf(
      "***********************************\n"
      "* AssetGen: C-Header Asset Encoder\n"
      "* Processing file: %s\n\n", file);

   byte *buff = readFullFile(file, &fSize);
   Tokenizer *tokens = tokenizerCreate((StringStream) { .pos = buff, .last = buff + fSize });
   tokenizerAcceptFile(tokens);
   
   checkedFree(buff);
   
   TokenStream tkstrm = { .pos = vecBegin(Token)(tokens->tokens),.last = vecEnd(Token)(tokens->tokens) };
   TokenStream *strm = &tkstrm;

   String *out = stringCreate("");


   while (!strmAtEnd(strm)) {
      Token *start = strmPeek(strm);

      Token *asset = _acceptAsset(strm);
      if (asset) {
         char *fName = c_str(asset->str);
         long fSize = 0;
         byte *buff = readFullFile(fName, &fSize);

         if (buff) {
            int lineSize = 40;
            int charsPerByte = 6; //0 x A A ,
            char *byteOutput = checkedCalloc(1, lineSize * charsPerByte * 2);
            int i = 0;
            char *outTracker = byteOutput;

            stringConcat(out, EncodeIdentifier);
            stringConcat(out, "(\"");
            stringConcat(out, fName);
            stringConcat(out, "\",\n   ");

            int lineCharCount = 0;
            for (i = 0; i < fSize; ++i) {
               byte b = buff[i];
               byte hi = b >> 4, lo = b & 15;
               sprintf(outTracker, "0x%c%c, ", hi > 9 ? hi - 10 + 'A' : hi + '0', lo > 9 ? lo - 10 + 'A' : lo + '0');
               outTracker += charsPerByte;

               if (lineCharCount++ >= lineSize) {
                  outTracker = byteOutput;
                  lineCharCount = 0;
                  stringConcat(out, byteOutput);
                  stringConcat(out, "\n   ");
               }
            }

            if (outTracker != byteOutput) {
               stringConcat(out, byteOutput);
            }
            
            stringConcat(out, "0x00)");

            checkedFree(byteOutput);
            checkedFree(buff);
            continue;
         }
         else {
            printf("ERROR: Unable to read %s for asset encoding!\n", fName);
            strm->pos = start;
         }
      }      

      stringConcat(out, c_str(strmPeek(strm)->raw));
      strmNext(strm);
   }

   FILE *fout = fopen(file, "wb");
   fputs(c_str(out), fout);
   fclose(fout);

   stringDestroy(out);
   tokenizerDestroy(tokens);
}
