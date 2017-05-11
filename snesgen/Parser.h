#pragma once

#include "libutils/String.h"
#include "libutils/Defs.h"

typedef struct {
   char *pos, *last;
}StringStream;

typedef enum {
   TOKEN_NULL = 0,
   TOKEN_IDENTIFIER,
   TOKEN_OPERATOR,//curly braces n shit
   TOKEN_FLOAT,
   TOKEN_INTEGER,
   TOKEN_STRING,
   TOKEN_CHAR,
   TOKEN_SKIPPABLE,
   TOKEN_COUNT
}TokenType;

typedef struct {
   TokenType type;
   union {
      char c;
      float f;
      int i;
      String *str;
   };
   String *raw;
}Token;

#define VectorTPart Token
#include "libutils/Vector_Decl.h"

typedef struct {
   StringStream strm;
   vec(Token) *tokens;
}Tokenizer;

Tokenizer *tokenizerCreate(StringStream strm);
void tokenizerDestroy(Tokenizer *self);

boolean tokenizerAcceptChar(Tokenizer *self);
boolean tokenizerAcceptDigit(Tokenizer *self);
boolean tokenizerAcceptIdentifierChar(Tokenizer *self);
boolean tokenizerAcceptIdentifier(Tokenizer *self);
boolean tokenizerAcceptPunctuation(Tokenizer *self);
boolean tokenizerAcceptWhitespace(Tokenizer *self);
boolean tokenizerAcceptCPPComment(Tokenizer *self);
boolean tokenizerAcceptCComment(Tokenizer *self);
boolean tokenizerAcceptComment(Tokenizer *self);
boolean tokenizerAcceptToken(Tokenizer *self);
boolean tokenizerAcceptSkippable(Tokenizer *self);
void tokenizerAcceptFile(Tokenizer *self);


typedef struct {
   Token *pos, *last;
}TokenStream;

boolean strmAtEnd(TokenStream *self);
Token *strmPeek(TokenStream *self);
void strmSkip(TokenStream *self, int skip);
void strmRewind(TokenStream *self, int skip);
void strmNext(TokenStream *self);
Token strmPop(TokenStream *self);
boolean strmAcceptIdentifier(TokenStream *self, String *id);
boolean strmAcceptIdentifierRaw(TokenStream *self, const char *id);
Token *strmAcceptIdentifierAny(TokenStream *self);
boolean strmAcceptOperator(TokenStream *self, char c);
Token *strmAcceptValueInteger(TokenStream *self);
Token *strmAcceptValueStringLiteral(TokenStream *self);
boolean strmAcceptSkippable(TokenStream *self);

