#include "Parser.h"


boolean sstrmAtEnd(StringStream *self) {
   return self->pos >= self->last;
}

char sstrmPeek(StringStream *self) {
   return *self->pos;
}

void sstrmSkip(StringStream *self, int skip) {
   self->pos += skip;
}

void sstrmRewind(StringStream *self, int skip) {
   self->pos -= skip;
}

void sstrmNext(StringStream *self) {
   ++self->pos;
}

char sstrmPop(StringStream *self) {
   return *self->pos++;
}

boolean sstrmAccept(StringStream *self, char c) {
   return *self->pos == c ? !!(++self->pos) : false;
}

boolean sstrmAcceptString(StringStream *self, const char *str) {
   const char *strStart = str;
   while ( *str) {
      if (sstrmAtEnd(self) || !sstrmAccept(self, *str)) {
         sstrmRewind(self, str - strStart);
         return false;
      }
      ++str;
   }
   return true;
}


#define STRM &self->strm

#define VectorTPart Token
#include "libutils/Vector_Impl.h"

static void _tokenDestroy(Token *self) {
   if (self->raw) {
      stringDestroy(self->raw);
   }
   if (self->type == TOKEN_STRING && self->str) {
      stringDestroy(self->str);
   }
}

Tokenizer *tokenizerCreate(StringStream strm) {
   Tokenizer *out = checkedCalloc(1, sizeof(Tokenizer));
   out->strm = strm;
   out->tokens = vecCreate(Token)(&_tokenDestroy);
   return out;
}

void tokenizerDestroy(Tokenizer *self) {
   vecDestroy(Token)(self->tokens);
   checkedFree(self);
}

boolean tokenizerAcceptChar(Tokenizer *self) {
   char c = sstrmPeek(STRM);

   if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      sstrmPop(STRM);
      return true;
   }

   return false;
}
boolean tokenizerAcceptDigit(Tokenizer *self) {
   char c = sstrmPeek(STRM);

   if ((c >= '0' && c <= '9')) {
      sstrmPop(STRM);
      return true;
   }

   return false;
}
boolean tokenizerAcceptIdentifierChar(Tokenizer *self) {
   return tokenizerAcceptChar(self) || sstrmAccept(STRM, '_');
}
boolean tokenizerAcceptIdentifier(Tokenizer *self) {
   char *start = self->strm.pos;

   if (!tokenizerAcceptIdentifierChar(self)) {
      return false;
   }

   while (!sstrmAtEnd(STRM)) {
      if (!tokenizerAcceptIdentifierChar(self) && !tokenizerAcceptDigit(self)) {
         break;
      }
   }

   int len = self->strm.pos - start;
   if (len) {
      Token t = { 0 };
      t.raw = stringCreate("");
      stringConcatEX(t.raw, start, len);

      t.type = TOKEN_IDENTIFIER;

      vecPushBack(Token)(self->tokens, &t);
   }
   return true;
}
boolean tokenizerAcceptPunctuation(Tokenizer *self) {
   static char chars[] = "~!@#%^&*().-+=:;<>,?|/\\{}[]";
   int i;
   for (i = 0; i < sizeof(chars); ++i) {

      if (!sstrmAtEnd(STRM) &&  sstrmAccept(STRM, chars[i])) {
         Token t = { 0 };
         t.raw = stringCreate("");
         stringConcatChar(t.raw, chars[i]);
         t.type = TOKEN_OPERATOR;
         t.c = chars[i];
         vecPushBack(Token)(self->tokens, &t);

         return true;
      }
   }
   return false;
}

boolean tokenizerAcceptValueChar(Tokenizer *self) {
   int rw = 0;
   char *start = self->strm.pos;

   if (sstrmAccept(STRM, '\'')) {

      sstrmAccept(STRM, '\\');
      sstrmNext(STRM);
      sstrmAccept(STRM, '\'');

      int len = self->strm.pos - start;
      if (len) {
         Token t = { 0 };
         t.raw = stringCreate("");
         stringConcatEX(t.raw, start, len);
         t.type = TOKEN_CHAR;

         vecPushBack(Token)(self->tokens, &t);
      }
      return true;
   }

   return false;
}
boolean tokenizerAcceptValueString(Tokenizer *self) {
   int rw = 0;
   char *start = self->strm.pos;

   if (sstrmAccept(STRM, '\"')) {
      while (!sstrmAtEnd(STRM)) {
         if (sstrmAccept(STRM, '\\')) {
            sstrmNext(STRM);
         }

         if (sstrmAccept(STRM, '\"')) {
            int len = self->strm.pos - start;
            if (len) {
               Token t = { 0 };
               t.raw = stringCreate("");
               stringConcatEX(t.raw, start, len);

               t.str = stringCreate("");
               stringConcatEX(t.str, start+1, len-2);

               t.type = TOKEN_STRING;

               vecPushBack(Token)(self->tokens, &t);
            }

            return true;
         }

         sstrmNext(STRM);
      }
   }

   return false;
}

boolean tokenizerAcceptValueNumber(Tokenizer *self) {
   boolean floatingPoint = false;
   char *start = self->strm.pos;

   if (sstrmAccept(STRM, '.')) {
      floatingPoint = true;
   }

   if (tokenizerAcceptDigit(self)) {
      while (!sstrmAtEnd(STRM)) {
         if (sstrmAccept(STRM, 'f')) {
            floatingPoint = true;
            break;
         }
         else if (sstrmAccept(STRM, '.')) {
            floatingPoint = true;
         }
         else if (!tokenizerAcceptDigit(self)) {
            break;
         }
      }

      int len = self->strm.pos - start;
      if (len) {
         Token t = { 0 };
         t.raw = stringCreate("");
         stringConcatEX(t.raw, start, len);

         if (floatingPoint) {
            t.type = TOKEN_FLOAT;
         }
         else {
            t.type = TOKEN_INTEGER;
         }
         vecPushBack(Token)(self->tokens, &t);
      }

      return true;
   }

   if (floatingPoint) {
      sstrmRewind(STRM, 1);//rw the period
   }
   return false;
}
boolean tokenizerAcceptValue(Tokenizer *self) {
   return tokenizerAcceptValueNumber(self) || tokenizerAcceptValueString(self) || tokenizerAcceptValueChar(self);
}
boolean tokenizerAcceptToken(Tokenizer *self) {
   return tokenizerAcceptValue(self) || tokenizerAcceptIdentifier(self) || tokenizerAcceptPunctuation(self);
}


boolean tokenizerAcceptWhitespace(Tokenizer *self) {
   static char chars[] = " \t\n\r";
   int i;
   for (i = 0; i < sizeof(chars); ++i) {
      if (sstrmAccept(STRM, chars[i])) {
         return true;
      }
   }
   return false;
}
boolean tokenizerAcceptCPPComment(Tokenizer *self) {
   if (!sstrmAcceptString(STRM, "//")) {
      return false;
   }

   while (!sstrmAtEnd(STRM)) {
      char c = sstrmPop(STRM);
      if (c == '\n') {
         break;
      }
   }

   return true;
}
boolean tokenizerAcceptCComment(Tokenizer *self) {
   if (!sstrmAcceptString(STRM, "/*")) {
      return false;
   }

   while (!sstrmAtEnd(STRM)) {
      if (sstrmAcceptString(STRM, "*/")) {
         break;
      }
      sstrmNext(STRM);
   }

   return true;
}
boolean tokenizerAcceptComment(Tokenizer *self) {
   return tokenizerAcceptCComment(self) || tokenizerAcceptCPPComment(self);
}
boolean tokenizerAcceptSkippable(Tokenizer *self) {
   boolean hit = false;

   char *start = self->strm.pos;

   while (!sstrmAtEnd(STRM) && (tokenizerAcceptComment(self) || tokenizerAcceptWhitespace(self))) { hit = true; }

   int len = self->strm.pos - start;
   if (len) {
      Token t = { 0 };
      t.raw = stringCreate("");
      stringConcatEX(t.raw, start, len);
      t.type = TOKEN_SKIPPABLE;
      vecPushBack(Token)(self->tokens, &t);
   }

   return hit;
}
void tokenizerAcceptFile(Tokenizer *self) {
   while (!sstrmAtEnd(STRM) && (tokenizerAcceptSkippable(self) || tokenizerAcceptToken(self))) { }
}


boolean strmAtEnd(TokenStream *self) {
   return self->pos >= self->last;
}
Token *strmPeek(TokenStream *self) {
   return self->pos;
}
void strmSkip(TokenStream *self, int skip) {
   self->pos += skip;
}
void strmRewind(TokenStream *self, int skip) {
   self->pos -= skip;
}
void strmNext(TokenStream *self) {
   ++self->pos;
}
Token strmPop(TokenStream *self) {
   return *self->pos++;
}
boolean strmAcceptIdentifier(TokenStream *self, String *id) {
   Token *t = strmPeek(self);

   if (t->type != TOKEN_IDENTIFIER) {
      return false;
   }
   else if (stringEqual(id, t->raw)) {
      strmNext(self);
      return true;
   }

   return false;
}

boolean strmAcceptIdentifierRaw(TokenStream *self, const char *id) {
   Token *t = strmPeek(self);

   if (t->type != TOKEN_IDENTIFIER) {
      return false;
   }
   else if (stringEqualRaw(t->raw, id)) {
      strmNext(self);
      return true;
   }

   return false;
}
Token *strmAcceptIdentifierAny(TokenStream *self) {
   Token *t = strmPeek(self);

   if (t->type != TOKEN_IDENTIFIER) {

      return NULL;
   }
   else {
      strmNext(self);
      return t;
   }
}
boolean strmAcceptOperator(TokenStream *self, char c) {
   Token *t = strmPeek(self);

   if (t->type != TOKEN_OPERATOR) {

      return false;
   }   
   else if (t->c == c) {
      strmNext(self);
      return true;
   }

   return false;
}

boolean strmAcceptSkippable(TokenStream *self) {
   if (strmPeek(self)->type == TOKEN_SKIPPABLE){
      strmNext(self);
      return true;
   }
   return false;
}
Token *strmAcceptValueStringLiteral(TokenStream *self) {
   Token *t = strmPeek(self);

   if (t->type != TOKEN_STRING) {
      return false;
   }
   else {
      strmNext(self);
      return t;
   }
}
Token *strmAcceptValueInteger(TokenStream *self) {
   Token *t = strmPeek(self);

   if (t->type != TOKEN_INTEGER) {
      return false;
   }
   else {
      strmNext(self);
      return t;
   }
}


