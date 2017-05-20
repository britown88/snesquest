#include "snesgen/Parser.h"
#include "libutils/Defs.h"
#include "libutils/CheckedMemory.h"
#include "libutils/BitBuffer.h"

#include <string.h>
#include <stdio.h>

typedef enum {
   CONSTRAINT_NO_ACTION = 0,
   CONSTRAINT_RESTRICT,
   CONSTRAINT_SET_NULL,
   CONSTRAINT_SET_DEFAULT,
   CONSTRAINT_CASCADE,
   CONSTRAINT_COUNT
}ConstraintBehavior;

static const char *ConstraintBehaviorStrings[] = {
   "NO_ACTION",
   "RESTRICT",
   "SET_NULL",
   "SET_DEFAULT",
   "CASCADE"
};

typedef enum {
   MODIFIER_PRIMARY_KEY = 0,
   MODIFIER_UNIQUE,
   MODIFIER_REPLACE_ON_CONFLICT,
   MODIFIER_AUTOINCREMENT,
   MODIFIER_SELECT,
   MODIFIER_NOT_NULL,
   MODIFIER_COUNT
}Modifier;

#define MOD(m) (1 << m)

static const char *ModifierStrings[] = {
   "PRIMARY_KEY",
   "UNIQUE",
   "REPLACE_ON_CONFLICT",
   "AUTOINCREMENT",
   "SELECT",
   "NOT_NULL"
};

typedef enum {
   TYPE_INT = 0,
   TYPE_STRING,
   TYPE_FLOAT,
   TYPE_BOOL,
   TYPE_BLOB,
   TYPE_CHAR,
   TYPE_COUNT
}MemberType;

static const char *MemberTypeStrings[] = {
   "int",
   "String",
   "float",
   "boolean",
   "Blob",
   "char"
};
static const char *MemberTypeCStrings[] = {
   "int64_t ",
   "const char *",
   "float ",
   "boolean ",
   "void *",
   "char "
};
static const char *MemberTypeSQLStrings[] = {
   "INTEGER",
   "STRING",
   "REAL",
   "BOOLEAN",
   "BLOB",
   "CHAR"
};


typedef struct {
   String *member, *parentTable, *parentMember;
   ConstraintBehavior behavior;
}DBConstraint;

void DBConstraintDestroy(DBConstraint *self) {
   stringDestroy(self->member);
   stringDestroy(self->parentTable);
   stringDestroy(self->parentMember);
}

#define VectorT DBConstraint
#include "libutils/Vector_Create.h"

typedef struct {
   int mods;//bitfield
   MemberType type;
   String *name;
}DBMember;

void DBMemberDestroy(DBMember *self) {
   stringDestroy(self->name);
}

#define VectorT DBMember
#include "libutils/Vector_Create.h"

typedef struct {
   String *name;
   vec(DBMember) *members;
   vec(DBConstraint) *constraints;
}DBStruct;

void DBStructDestroy(DBStruct *self) {
   vecDestroy(DBMember)(self->members);
   vecDestroy(DBConstraint)(self->constraints);
   stringDestroy(self->name);
}

#define VectorT DBStruct
#include "libutils/Vector_Create.h"

typedef struct {
   TokenStream strm;
   vec(DBStruct) *structs;
}DBGenLexer;

#define STRM &self->strm
#define SKIP_SKIPPABLES(_STREAM) while (strmAcceptSkippable(_STREAM)) {}

DBGenLexer *dbGenLexerCreate(TokenStream strm) {
   DBGenLexer *out = checkedCalloc(1, sizeof(DBGenLexer));
   out->strm = strm;
   out->structs = vecCreate(DBStruct)(&DBStructDestroy);
   return out;
}

void dbGenLexerDestroy(DBGenLexer *self) {
   vecDestroy(DBStruct)(self->structs);
   checkedFree(self);
}

MemberType lexerAcceptType(DBGenLexer *self) {
   int i = 0;

   for (i = 0; i < TYPE_COUNT; ++i) {
      if (strmAcceptIdentifierRaw(STRM, MemberTypeStrings[i])) {
         return i;
      }
   }

   return TYPE_COUNT;
}
ConstraintBehavior lexerAcceptConstraintBehavior(DBGenLexer *self) {
   int i = 0;

   for (i = 0; i < CONSTRAINT_COUNT; ++i) {
      if (strmAcceptIdentifierRaw(STRM, ConstraintBehaviorStrings[i])) {
         return i;
      }
   }

   return CONSTRAINT_COUNT;
}


Modifier lexerAcceptModifier(DBGenLexer *self) {
   int i = 0;

   SKIP_SKIPPABLES(STRM);

   for (i = 0; i < MODIFIER_COUNT; ++i) {
      if (strmAcceptIdentifierRaw(STRM, ModifierStrings[i])) {
         return i;
      }
   }

   return MODIFIER_COUNT;
}
DBConstraint lexerAcceptConstraint(DBGenLexer *self) {
   static DBConstraint NullOut = { 0 };
   DBConstraint newConstraint = { 0 };

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptIdentifierRaw(STRM, "FOREIGN_KEY")) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptOperator(STRM, '(')) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   Token *member = strmAcceptIdentifierAny(STRM);
   if (!member) {
      return NullOut;
   }
   newConstraint.member = stringCopy(member->raw);

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptOperator(STRM, ',')) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   Token *parentTable = strmAcceptIdentifierAny(STRM);
   if (!parentTable) {
      return NullOut;
   }
   newConstraint.parentTable = stringCopy(parentTable->raw);

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptOperator(STRM, '.')) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   Token *parentMember = strmAcceptIdentifierAny(STRM);
   if (!parentMember) {
      return NullOut;
   }
   newConstraint.parentMember = stringCopy(parentMember->raw);

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptOperator(STRM, ',')) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   newConstraint.behavior = lexerAcceptConstraintBehavior(self);
   if (newConstraint.behavior == CONSTRAINT_COUNT) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptOperator(STRM, ')')) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptOperator(STRM, ';')) {
      return NullOut;
   }

   return newConstraint;
}
DBMember lexerAcceptMember(DBGenLexer *self) {
   static DBMember NullOut = { .name = NULL };
   Modifier newMod;

   DBMember newMember = { 0 };

   SKIP_SKIPPABLES(STRM);

   while ((newMod = lexerAcceptModifier(self)) != MODIFIER_COUNT) {
      newMember.mods |= MOD(newMod);// :D
      SKIP_SKIPPABLES(STRM);
   }

   newMember.type = lexerAcceptType(self);
   if (newMember.type == TYPE_COUNT) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);


   Token *name = strmAcceptIdentifierAny(STRM);
   if (!name) {
      return NullOut;
   }
   newMember.name = stringCopy(name->raw);
   if (!newMember.name) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptOperator(STRM, ';')) {
      return NullOut;
   }

   return newMember;
}

static void _destroyDBMember(DBStruct *self) {
   stringDestroy(self->name);
   vecDestroy(DBMember)(self->members);
   vecDestroy(DBConstraint)(self->constraints);
}

DBStruct lexerAcceptStruct(DBGenLexer *self) {
   static DBStruct NullOut = { .name = NULL };
   static const char *svStruct = "table";
   DBMember newMember = { 0 };
   DBStruct newStruct = { 0 };
   DBConstraint newConstraint = { 0 };

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptIdentifierRaw(STRM, svStruct)) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   Token *id = strmAcceptIdentifierAny(STRM);
   if (!id) {
      return NullOut;
   }
   newStruct.name = stringCopy(id->raw);

   if (!newStruct.name) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptOperator(STRM, '{')) {
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   newStruct.members = vecCreate(DBMember)(&DBMemberDestroy);
   while ((newMember = lexerAcceptMember(self)).name) {
      vecPushBack(DBMember)(newStruct.members, &newMember);
      SKIP_SKIPPABLES(STRM);
   }

   SKIP_SKIPPABLES(STRM);

   newStruct.constraints = vecCreate(DBConstraint)(&DBConstraintDestroy);
   if (strmAcceptIdentifierRaw(STRM, "constraints")) {
      SKIP_SKIPPABLES(STRM);
      if (!strmAcceptOperator(STRM, '{')) {
         
         return NullOut;
      }

      while ((newConstraint = lexerAcceptConstraint(self)).member) {
         vecPushBack(DBConstraint)(newStruct.constraints, &newConstraint);
         SKIP_SKIPPABLES(STRM);
      }

      SKIP_SKIPPABLES(STRM);
      if (!strmAcceptOperator(STRM, '}')) {
         _destroyDBMember(&newStruct);
         return NullOut;
      }

      SKIP_SKIPPABLES(STRM);
      if (!strmAcceptOperator(STRM, ';')) {
         _destroyDBMember(&newStruct);
         return NullOut;
      }

      SKIP_SKIPPABLES(STRM);
   }


   if (!strmAcceptOperator(STRM, '}')) {
      _destroyDBMember(&newStruct);
      return NullOut;
   }

   SKIP_SKIPPABLES(STRM);

   if (!strmAcceptOperator(STRM, ';')) {
      _destroyDBMember(&newStruct);
      return NullOut;
   }

   return newStruct;

}
void lexerAcceptFile(DBGenLexer *self) {
   DBStruct newStruct;
   while ((newStruct = lexerAcceptStruct(self)).name) {
      vecPushBack(DBStruct)(self->structs, &newStruct);
   }
}

#undef STRM

typedef struct {
   String *dir, *input, *inputFileOnly, *outputh, *outputc;
   vec(DBStruct) *structs;
}FileData;


void headerWriteHeader(FILE *f, FileData *data) {
   fprintf(f,
      "/***********************************************************************\n"
      "   WARNING: This file generated by robots.  Do not attempt to modify.\n\n"
      "   This API is for use with %s.db\n"
      "   Which contains %i table(s).\n"
      "***********************************************************************/\n\n"
      "#pragma once\n\n"
      "#include \"libutils/Defs.h\"\n"
      "#include \"libutils/String.h\"\n\n"
      "typedef struct sqlite3_stmt sqlite3_stmt;\n"
      "typedef struct DB_%s DB_%s;\n"

      "\n",
      c_str(data->inputFileOnly), //.db
      vecSize(DBStruct)(data->structs), //x tables
      c_str(data->inputFileOnly), c_str(data->inputFileOnly)
      );
}
void headerCreateStruct(FILE *f, FileData *fd, DBStruct *strct) {

   //the struct
   fprintf(f, "typedef struct {\n");
   vecForEach(DBMember, mem, strct->members, {
      switch (mem->type) {
      case TYPE_BLOB: fprintf(f,    "   void *%s;\n   int %sSize;\n", c_str(mem->name), c_str(mem->name)); break;
      case TYPE_INT: fprintf(f,     "   int64_t %s;\n", c_str(mem->name)); break;
      case TYPE_STRING: fprintf(f,  "   String *%s;\n", c_str(mem->name)); break;
      case TYPE_BOOL: fprintf(f,    "   boolean %s;\n", c_str(mem->name)); break;
      case TYPE_CHAR: fprintf(f,    "   char %s;\n", c_str(mem->name)); break;
      case TYPE_FLOAT: fprintf(f,   "   float %s;\n", c_str(mem->name)); break;
      }
   });
   fprintf(f, "} DB%s;\n\n", c_str(strct->name));

   //vector
   fprintf(f, "#define VectorTPart DB%s\n#include \"libutils/Vector_Decl.h\"\n\n", c_str(strct->name));

   //functions
   fprintf(f, "void db%sDestroy(DB%s *self); //this does not call free on self!!\n", c_str(strct->name), c_str(strct->name));
   fprintf(f, "int db%sInsert(DB_%s *db, DB%s *obj);\n", c_str(strct->name), c_str(fd->inputFileOnly), c_str(strct->name));
   fprintf(f, "int db%sUpdate(DB_%s *db, const DB%s *obj); //will base on primary key\n", c_str(strct->name), c_str(fd->inputFileOnly), c_str(strct->name));
   fprintf(f, "vec(DB%s) *db%sSelectAll(DB_%s *db);\n", c_str(strct->name), c_str(strct->name), c_str(fd->inputFileOnly));

   vecForEach(DBMember, member, strct->members, {
      if (member->mods&MOD(MODIFIER_SELECT)) {

         fprintf(f, "DB%s db%sSelectFirstBy%s(DB_%s *db, %s%s);\n",
            c_str(strct->name), c_str(strct->name), c_str(member->name),
            c_str(fd->inputFileOnly), MemberTypeCStrings[member->type], c_str(member->name));

         if (!(member->mods&MOD(MODIFIER_UNIQUE))) {
            fprintf(f, "vec(DB%s) *db%sSelectBy%s(DB_%s *db, %s%s);\n",
               c_str(strct->name), c_str(strct->name), c_str(member->name),
               c_str(fd->inputFileOnly), MemberTypeCStrings[member->type], c_str(member->name));
         }
      }
   });

   fprintf(f, "int db%sDeleteAll(DB_%s *db);\n", c_str(strct->name), c_str(fd->inputFileOnly));

   vecForEach(DBMember, member, strct->members, {
      if (member->mods&MOD(MODIFIER_SELECT)) {

         fprintf(f, "int db%sDeleteBy%s(DB_%s *db, %s%s);\n",
            c_str(strct->name), c_str(member->name),
            c_str(fd->inputFileOnly), MemberTypeCStrings[member->type], c_str(member->name));

      }
   });

   fprintf(f, "\n");
}

void headerCreateDB(FILE *f, FileData *fd) {
   fprintf(f, "DB_%s *db_%sCreate();\n", c_str(fd->inputFileOnly), c_str(fd->inputFileOnly));
   fprintf(f, "void db_%sDestroy(DB_%s *self);\n", c_str(fd->inputFileOnly), c_str(fd->inputFileOnly));
   fprintf(f, "int db_%sCreateTables(DB_%s *self);\n\n", c_str(fd->inputFileOnly), c_str(fd->inputFileOnly));
}

void createHeader(FileData *data) {
   FILE *f = fopen(c_str(data->outputh), "w");

   if (!f) {
      printf("ERROR: Unable to create file %s.\n", c_str(data->outputh));
      return;
   }

   headerWriteHeader(f, data);

   headerCreateDB(f, data);

   vecForEach(DBStruct, strct, data->structs, {
      headerCreateStruct(f, data, strct);
   });



   fclose(f);
}

void sourceWriteHeader(FILE *f, FileData *data) {
   fprintf(f,
      "/***********************************************************************\n"
      "   WARNING: This file generated by robots.  Do not attempt to modify.\n\n"
      "   This API is for use with %s.db\n"
      "   Which contains %i table(s).\n"
      "***********************************************************************/\n\n"
      "#include \"%s.h\"\n"
      "#include \"DB.h\"\n"
      "#include \"libutils/CheckedMemory.h\"\n"

      "#include \"sqlite/sqlite3.h\"\n\n"

      , c_str(data->inputFileOnly), vecSize(DBStruct)(data->structs), c_str(data->inputFileOnly)

      );
}

void sourceWriteDestroy(FILE *f, FileData *fd, DBStruct *strct) {
   fprintf(f, "void db%sDestroy(DB%s *self){\n", c_str(strct->name), c_str(strct->name));

   vecForEach(DBMember, member, strct->members, {
      if (member->type == TYPE_STRING) {
         fprintf(f,
            "   if(self->%s){\n"
            "      stringDestroy(self->%s);\n"
            "      self->%s = NULL;\n"
            "   }\n", c_str(member->name), c_str(member->name), c_str(member->name));
      }
      else if (member->type == TYPE_BLOB) {
         fprintf(f,
            "   if(self->%s){\n"
            "      checkedFree(self->%s);\n"
            "      self->%s = NULL;\n"
            "   }\n", c_str(member->name), c_str(member->name), c_str(member->name));
      }
   });

   fprintf(f, "}\n");
}


void sourceWriteBindMember(FILE *f, FileData *fd, DBStruct *strct, DBMember *member, const char *stmt, size_t index) {

   switch (member->type) {
   case TYPE_INT:
   case TYPE_BOOL:
   case TYPE_CHAR:
      fprintf(f, "   result = sqlite3_bind_int64(db->%sStmts.%s, %i, obj->%s);\n",
         c_str(strct->name), stmt, index, c_str(member->name));
      break;
   case TYPE_BLOB:
      fprintf(f, "   result = sqlite3_bind_blob(db->%sStmts.%s, %i, obj->%s, obj->%sSize, SQLITE_TRANSIENT);\n",
         c_str(strct->name), stmt, index, c_str(member->name), c_str(member->name));
      break;
   case TYPE_FLOAT:
      fprintf(f, "   result = sqlite3_bind_double(db->%sStmts.%s, %i, (double)obj->%s);\n",
         c_str(strct->name), stmt, index, c_str(member->name));
      break;
   case TYPE_STRING:
      fprintf(f, "   result = sqlite3_bind_text(db->%sStmts.%s, %i, c_str(obj->%s), -1, NULL);\n",
         c_str(strct->name), stmt, index, c_str(member->name));
      break;
   }

   fprintf(f,
      "   if (result != SQLITE_OK) {\n"
      "      stringSet(db->base.err, sqlite3_errmsg(db->base.conn));\n"
      "      return DB_FAILURE;\n"
      "   }\n\n");
}

void sourceWriteBindMemberSelect(FILE *f, FileData *fd, DBStruct *strct, DBMember *member, const char *stmt, size_t index, const char *outName) {

   switch (member->type) {
   case TYPE_INT:
   case TYPE_BOOL:
   case TYPE_CHAR:
      fprintf(f, "   result = sqlite3_bind_int64(db->%sStmts.%s, %i, (int)%s);\n",
         c_str(strct->name), stmt, index, c_str(member->name));
      break;
   case TYPE_BLOB:
      fprintf(f, "   result = sqlite3_bind_blob(db->%sStmts.%s, %i, %s, %sSize, SQLITE_TRANSIENT);\n",
         c_str(strct->name), stmt, index, c_str(member->name), c_str(member->name));
      break;
   case TYPE_FLOAT:
      fprintf(f, "   result = sqlite3_bind_double(db->%sStmts.%s, %i, (double)%s);\n",
         c_str(strct->name), stmt, index, c_str(member->name));
      break;
   case TYPE_STRING:
      fprintf(f, "   result = sqlite3_bind_text(db->%sStmts.%s, %i, %s, -1, NULL);\n",
         c_str(strct->name), stmt, index, c_str(member->name));
      break;
   }

   fprintf(f,
      "   if (result != SQLITE_OK) {\n"
      "      stringSet(db->base.err, sqlite3_errmsg(db->base.conn));\n"
      "      return %s;\n"
      "   }\n\n", outName);
}

void sourceWriteGetColumn(FILE *f, FileData *fd, DBStruct *strct, DBMember *member, const char *stmt, size_t index, const char *objName) {

   switch (member->type) {
   case TYPE_INT:
   case TYPE_BOOL:
      fprintf(f, "      %s.%s = sqlite3_column_int(db->%sStmts.%s, %i);\n",
         objName, c_str(member->name), c_str(strct->name), stmt, index);
      break;
   case TYPE_CHAR:
      fprintf(f, "      %s.%s = (char)sqlite3_column_int(db->%sStmts.%s, %i);\n",
         objName, c_str(member->name), c_str(strct->name), stmt, index);
      break;
   case TYPE_BLOB:
      fprintf(f, "      %s.%sSize = sqlite3_column_bytes(db->%sStmts.%s, %i);\n",
         objName, c_str(member->name), c_str(strct->name), stmt, index);

      fprintf(f,
         "      %s.%s = checkedCalloc(1, %s.%sSize);\n"
         "      memcpy(%s.%s, sqlite3_column_blob(db->%sStmts.%s, %i), %s.%sSize);\n",
         objName, c_str(member->name), objName, c_str(member->name), objName, c_str(member->name), c_str(strct->name), stmt, index, objName, c_str(member->name));


      break;
   case TYPE_FLOAT:
      fprintf(f, "      %s.%s = (float)sqlite3_column_double(db->%sStmts.%s, %i);\n",
         objName, c_str(member->name), c_str(strct->name), stmt, index);
      break;
   case TYPE_STRING:
      fprintf(f, "      %s.%s = stringCreate(sqlite3_column_text(db->%sStmts.%s, %i));\n",
         objName, c_str(member->name), c_str(strct->name), stmt, index);
      break;
   }



}

void sourceWriteStatementDestroy(FILE *f, const char *table, const char *stmt) {
   fprintf(f,
      "   if(db->%sStmts.%s){\n"
      "      sqlite3_finalize(db->%sStmts.%s);\n"
      "      db->%sStmts.%s = NULL;\n"
      "   }\n", table, stmt, table, stmt, table, stmt);
}
void sourceWriteDestroyStatements(FILE *f, FileData *fd, DBStruct *strct) {
   static char buff[256] = { 0 };
   fprintf(f, "void db%sDestroyStatements(DB_%s *db){\n", c_str(strct->name), c_str(fd->inputFileOnly));

   sourceWriteStatementDestroy(f, c_str(strct->name), "insert");
   sourceWriteStatementDestroy(f, c_str(strct->name), "update");
   sourceWriteStatementDestroy(f, c_str(strct->name), "selectAll");
   sourceWriteStatementDestroy(f, c_str(strct->name), "deleteAll");

   vecForEach(DBMember, member, strct->members, {
      if (member->mods&MOD(MODIFIER_SELECT)) {
         sprintf(buff, "selectBy%s", c_str(member->name));
         sourceWriteStatementDestroy(f, c_str(strct->name), buff);

         sprintf(buff, "deleteBy%s", c_str(member->name));
         sourceWriteStatementDestroy(f, c_str(strct->name), buff);
      }
   });

   fprintf(f, "}\n");
}
void sourceWriteCreateTable(FILE *f, FileData *fd, DBStruct *strct) {
   int i = 0;
   boolean first = true;

   fprintf(f, "int db%sCreateTable(DB_%s *db){\n", c_str(strct->name), c_str(fd->inputFileOnly));

   //the table creation string
   fprintf(f,
      "   static const char *cmd = \"CREATE TABLE \\\"%s\\\" (", c_str(strct->name));
   vecForEach(DBMember, member, strct->members, {
      if (!first) {
         fprintf(f, ", ");
      }
      else {
         first = false;
      }
      fprintf(f, "\\\"%s\\\"", c_str(member->name));
      fprintf(f, " %s", MemberTypeSQLStrings[member->type]);

      if (member->mods&MOD(MODIFIER_PRIMARY_KEY)) { fprintf(f, " PRIMARY KEY"); }
      if (member->mods&MOD(MODIFIER_AUTOINCREMENT)) { fprintf(f, " AUTOINCREMENT"); }
      if (member->mods&MOD(MODIFIER_UNIQUE)) { fprintf(f, " UNIQUE"); }
      if (member->mods&MOD(MODIFIER_REPLACE_ON_CONFLICT)) { fprintf(f, " ON CONFLICT REPLACE"); }
      if (member->mods&MOD(MODIFIER_NOT_NULL)) { fprintf(f, " NOT NULL"); }

   });

   vecForEach(DBConstraint, constraint, strct->constraints, {
      fprintf(f, ", FOREIGN KEY (\\\"%s\\\") REFERENCES \\\"%s\\\" (\\\"%s\\\") ON DELETE ",
                                    c_str(constraint->member), c_str(constraint->parentTable), c_str(constraint->parentMember));

      switch (constraint->behavior) {
      case CONSTRAINT_NO_ACTION: fprintf(f, "NO ACTION"); break;
      case CONSTRAINT_RESTRICT: fprintf(f, "RESTRICT"); break;
      case CONSTRAINT_SET_NULL: fprintf(f, "SET NULL"); break;
      case CONSTRAINT_SET_DEFAULT: fprintf(f, "SET DEFAULT"); break;
      case CONSTRAINT_CASCADE: fprintf(f, "CASCADE"); break;

      }
      

   });

   fprintf(f, ");\";\n");

   fprintf(f,
      "   return dbExecute((DBBase*)db, cmd);\n"
      );

   fprintf(f, "}\n");
}
void sourceWriteInsert(FILE *f, FileData *fd, DBStruct *strct) {
   boolean first = true;
   int i = 0;
   DBMember *pkey = NULL;
   fprintf(f, "int db%sInsert(DB_%s *db, DB%s *obj){\n", c_str(strct->name), c_str(fd->inputFileOnly), c_str(strct->name));

   fprintf(f,
      "   int result = 0;\n"
      "   static const char *stmt = \"INSERT INTO \\\"%s\\\" ("
      , c_str(strct->name));

   vecForEach(DBMember, member, strct->members, {
      if (member->mods&MOD(MODIFIER_PRIMARY_KEY)) {
         pkey = member;
      }

      if (!(member->mods&MOD(MODIFIER_AUTOINCREMENT))) {
         if (!first) {
            fprintf(f, ", ");
         }
         else {
            first = false;
         }
         fprintf(f, "\\\"%s\\\"", c_str(member->name));
      }
   });

   fprintf(f, ") VALUES (");

   first = true;

   vecForEach(DBMember, member, strct->members, {
      if (!(member->mods&MOD(MODIFIER_AUTOINCREMENT))) {
         if (!first) {
            fprintf(f, ", ");
         }
         else {
            first = false;
         }
         fprintf(f, ":%s", c_str(member->name));
      }
   });

   fprintf(f, ");\";\n");
   fprintf(f,
      "   if(dbPrepareStatement((DBBase*)db, &db->%sStmts.insert, stmt) != DB_SUCCESS){\n"
      "      stringSet(db->base.err, sqlite3_errmsg(db->base.conn));\n"
      "      return DB_FAILURE;\n"
      "   }\n\n"
      "   //bind the values\n", c_str(strct->name)
      );

   i = 1;
   vecForEach(DBMember, member, strct->members, {
      if (!(member->mods&MOD(MODIFIER_AUTOINCREMENT))) {
         sourceWriteBindMember(f, fd, strct, member, "insert", i++);
      }
   });

   //now run it
   fprintf(f,
      "   //now run it\n"
      "   result = sqlite3_step(db->%sStmts.insert);\n"
      "   if (result != SQLITE_DONE) {\n"
      "      stringSet(db->base.err, sqlite3_errmsg(db->base.conn));\n"
      "      return DB_FAILURE;\n"
      "   }\n\n", c_str(strct->name)
      );

   // if our pkey autoincrements, we need to retrieve the new value   
   if (pkey && pkey->mods&MOD(MODIFIER_AUTOINCREMENT) && pkey->type == TYPE_INT) {
      fprintf(f, 
      "   //Fill the inserted record's id with its id from the db\n"
      "   obj->%s = sqlite3_last_insert_rowid(db->base.conn);\n\n",
         c_str(pkey->name)
      );

   }


   fprintf(f, 
      "   return DB_SUCCESS;\n"
      "}\n");
}
void sourceWriteUpdate(FILE *f, FileData *fd, DBStruct *strct) {
   boolean first = true;
   int i = 0;
   DBMember *pkey = NULL;
   fprintf(f, "int db%sUpdate(DB_%s *db, const DB%s *obj){\n", c_str(strct->name), c_str(fd->inputFileOnly), c_str(strct->name));

   fprintf(f,
      "   int result = 0;\n"
      "   static const char *stmt = \"UPDATE \\\"%s\\\" SET ("
      , c_str(strct->name));

   vecForEach(DBMember, member, strct->members, {
      if (member->mods&MOD(MODIFIER_PRIMARY_KEY)) {
         pkey = member;
         continue;
      }

   if (!(member->mods&MOD(MODIFIER_AUTOINCREMENT))) {

      if (!first) {
         fprintf(f, ", ");
      }
      else {
         first = false;
      }
      fprintf(f, "\\\"%s\\\" = :%s", c_str(member->name), c_str(member->name));
   }
   });

   fprintf(f, ") WHERE (\\\"%s\\\" = :%s)\";\n", c_str(pkey->name), c_str(pkey->name));


   fprintf(f,
      "   if(dbPrepareStatement((DBBase*)db, &db->%sStmts.update, stmt) != DB_SUCCESS){\n"
      "      return DB_FAILURE;\n"
      "   }\n\n"
      "   //bind the values\n", c_str(strct->name)
      );

   i = 1;
   vecForEach(DBMember, member, strct->members, {
      if (!(member->mods&MOD(MODIFIER_AUTOINCREMENT)) && !(member->mods&MOD(MODIFIER_PRIMARY_KEY))) {
         sourceWriteBindMember(f, fd, strct, member, "update", i++);
      }
   });

   //and the pkey
   fprintf(f, "   //primary key:\n");
   sourceWriteBindMember(f, fd, strct, pkey, "update", i++);

   //now run it
   fprintf(f,
      "   //now run it\n"
      "   result = sqlite3_step(db->%sStmts.update);\n"
      "   if (result != SQLITE_DONE) {\n"
      "      stringSet(db->base.err, sqlite3_errmsg(db->base.conn));\n"
      "      return DB_FAILURE;\n"
      "   }\n\n"
      "   return DB_SUCCESS;\n", c_str(strct->name)
      );

   fprintf(f, "}\n");
}
void sourceWriteSelectAll(FILE *f, FileData *fd, DBStruct *strct) {


   boolean first = true;
   int i = 0;
   fprintf(f, "vec(DB%s) *db%sSelectAll(DB_%s *db){\n", c_str(strct->name), c_str(strct->name), c_str(fd->inputFileOnly));

   fprintf(f,
      "   int result = 0;\n"
      "   static const char *stmt = \"SELECT * FROM \\\"%s\\\";\";\n", c_str(strct->name));

   fprintf(f,
      "   if(dbPrepareStatement((DBBase*)db, &db->%sStmts.selectAll, stmt) != DB_SUCCESS){\n"
      "      return NULL;\n"
      "   }\n\n", c_str(strct->name)
      );

   fprintf(f,
      "   vec(DB%s) *out = vecCreate(DB%s)(&db%sDestroy);\n\n"

      "   while((result = sqlite3_step(db->%sStmts.selectAll)) == SQLITE_ROW){\n"
      "      DB%s newObj = {0};\n\n"
      , c_str(strct->name), c_str(strct->name), c_str(strct->name), c_str(strct->name), c_str(strct->name)
      );


   i = 0;
   vecForEach(DBMember, member, strct->members, {
      sourceWriteGetColumn(f, fd, strct, member, "selectAll", i++, "newObj");
   });

   fprintf(f,
      "      \n"
      "      vecPushBack(DB%s)(out, &newObj);\n\n"
      "   };\n\n"

      "   if(result != SQLITE_DONE){\n"
      "      vecDestroy(DB%s)(out);\n"
      "      return NULL;\n"
      "   }\n\n"
      "   return out;\n", c_str(strct->name), c_str(strct->name));

   fprintf(f, "}\n");
}
void sourceWriteSelectFirst(FILE *f, FileData *fd, DBStruct *strct, DBMember *member) {

   boolean first = true;
   int i = 0;
   char stmtName[256] = { 0 };
   sprintf(stmtName, "selectBy%s", c_str(member->name));

   fprintf(f, "DB%s db%sSelectFirstBy%s(DB_%s *db, %s%s){\n",
      c_str(strct->name), c_str(strct->name), c_str(member->name),
      c_str(fd->inputFileOnly), MemberTypeCStrings[member->type], c_str(member->name));

   fprintf(f,
      "   DB%s out = {0};\n"
      "   int result = 0;\n"
      "   static const char *stmt = \"SELECT * FROM \\\"%s\\\" WHERE \\\"%s\\\" = :%s;\";\n", c_str(strct->name), c_str(strct->name), c_str(member->name), c_str(member->name));

   fprintf(f,
      "   if(dbPrepareStatement((DBBase*)db, &db->%sStmts.%s, stmt) != DB_SUCCESS){\n"
      "      return out;\n"
      "   }\n\n", c_str(strct->name), stmtName
      );

   sourceWriteBindMemberSelect(f, fd, strct, member, stmtName, 1, "out");

   fprintf(f,
      "   if((result = sqlite3_step(db->%sStmts.%s)) == SQLITE_ROW){\n"

      , c_str(strct->name), stmtName
      );


   i = 0;
   vecForEach(DBMember, member, strct->members, {
      sourceWriteGetColumn(f, fd, strct, member, stmtName, i++, "out");
   });

   fprintf(f,
      "   };\n\n"
      "   return out;\n");

   fprintf(f, "}\n");
}
void sourceWriteSelectBy(FILE *f, FileData *fd, DBStruct *strct, DBMember *member) {



   boolean first = true;
   int i = 0;
   char stmtName[256] = { 0 };
   sprintf(stmtName, "selectBy%s", c_str(member->name));

   fprintf(f, "vec(DB%s) *db%sSelectBy%s(DB_%s *db, %s%s){\n",
      c_str(strct->name), c_str(strct->name), c_str(member->name),
      c_str(fd->inputFileOnly), MemberTypeCStrings[member->type], c_str(member->name));

   fprintf(f,
      "   int result = 0;\n"
      "   vec(DB%s) *out = NULL;\n"
      "   static const char *stmt = \"SELECT * FROM \\\"%s\\\" WHERE \\\"%s\\\" = :%s;\";\n"
      , c_str(strct->name), c_str(strct->name), c_str(member->name), c_str(member->name));

   fprintf(f,
      "   if(dbPrepareStatement((DBBase*)db, &db->%sStmts.%s, stmt) != DB_SUCCESS){\n"
      "      return NULL;\n"
      "   }\n\n", c_str(strct->name), stmtName
      );

   sourceWriteBindMemberSelect(f, fd, strct, member, stmtName, 1, "out");

   fprintf(f,
      "   out = vecCreate(DB%s)(&db%sDestroy);\n\n"

      "   while((result = sqlite3_step(db->%sStmts.%s)) == SQLITE_ROW){\n"
      "      DB%s newObj = {0};\n\n"
      , c_str(strct->name), c_str(strct->name), c_str(strct->name), stmtName, c_str(strct->name)
      );


   i = 0;
   vecForEach(DBMember, member, strct->members, {
      sourceWriteGetColumn(f, fd, strct, member, stmtName, i++, "newObj");
   });

   fprintf(f,
      "      \n"
      "      vecPushBack(DB%s)(out, &newObj);\n\n"
      "   };\n\n"

      "   if(result != SQLITE_DONE){\n"
      "      vecDestroy(DB%s)(out);\n"
      "      return NULL;\n"
      "   }\n\n"
      "   return out;\n", c_str(strct->name), c_str(strct->name));

   fprintf(f, "}\n");
}
void sourceWriteDeleteAll(FILE *f, FileData *fd, DBStruct *strct) {
   fprintf(f, "int db%sDeleteAll(DB_%s *db){\nreturn DB_SUCCESS;\n}\n", c_str(strct->name), c_str(fd->inputFileOnly));
}
void sourceWriteDeleteBy(FILE *f, FileData *fd, DBStruct *strct, DBMember *member) {


   boolean first = true;
   int i = 0;
   char stmtName[256] = { 0 };

   sprintf(stmtName, "deleteBy%s", c_str(member->name));

   fprintf(f, "int db%sDeleteBy%s(DB_%s *db, %s%s){\n",
      c_str(strct->name), c_str(member->name),
      c_str(fd->inputFileOnly), MemberTypeCStrings[member->type], c_str(member->name));

   fprintf(f,
      "   int result = 0;\n"
      "   static const char *stmt = \"DELETE FROM \\\"%s\\\" WHERE (\\\"%s\\\" = :%s);\";\n"
      , c_str(strct->name), c_str(member->name), c_str(member->name));

   fprintf(f,
      "   if(dbPrepareStatement((DBBase*)db, &db->%sStmts.%s, stmt) != DB_SUCCESS){\n"
      "      return DB_FAILURE;\n"
      "   }\n\n", c_str(strct->name), stmtName
      );


   //and the pkey
   fprintf(f, "   //primary key:\n");
   sourceWriteBindMemberSelect(f, fd, strct, member, stmtName, 1, "DB_FAILURE");

   //now run it
   fprintf(f,
      "   //now run it\n"
      "   result = sqlite3_step(db->%sStmts.%s);\n"
      "   if (result != SQLITE_DONE) {\n"
      "      stringSet(db->base.err, sqlite3_errmsg(db->base.conn));\n"
      "      return DB_FAILURE;\n"
      "   }\n\n"
      "   return DB_SUCCESS;\n", c_str(strct->name), stmtName
      );

   fprintf(f, "}\n");
}


void sourceCreateStruct(FILE *f, FileData *fd, DBStruct *strct) {

   fprintf(f, "#define VectorTPart DB%s\n#include \"libutils/Vector_Impl.h\"\n\n", c_str(strct->name));

   sourceWriteDestroy(f, fd, strct);
   sourceWriteDestroyStatements(f, fd, strct);
   sourceWriteCreateTable(f, fd, strct);
   sourceWriteInsert(f, fd, strct);
   sourceWriteUpdate(f, fd, strct);
   sourceWriteSelectAll(f, fd, strct);

   vecForEach(DBMember, member, strct->members, {
      if (member->mods&MOD(MODIFIER_SELECT)) {

         sourceWriteSelectFirst(f, fd, strct, member);

         if (!(member->mods&MOD(MODIFIER_UNIQUE))) {
            sourceWriteSelectBy(f, fd, strct, member);
         }
      }
   });

   sourceWriteDeleteAll(f, fd, strct);

   vecForEach(DBMember, member, strct->members, {
      if (member->mods&MOD(MODIFIER_SELECT)) {
         sourceWriteDeleteBy(f, fd, strct, member);
      }
   });
}

void sourceCreateDB(FILE *f, FileData *fd) {

   vecForEach(DBStruct, strct, fd->structs, {
      fprintf(f, "void db%sDestroyStatements(DB_%s *db);\n", c_str(strct->name), c_str(fd->inputFileOnly));
   fprintf(f, "int db%sCreateTable(DB_%s *db);\n", c_str(strct->name), c_str(fd->inputFileOnly));
   });

   fprintf(f, "\n");


   vecForEach(DBStruct, strct, fd->structs, {
      //statements struct
      fprintf(f, "typedef struct {\n");
   fprintf(f, "   sqlite3_stmt *insert;\n");
   fprintf(f, "   sqlite3_stmt *update;\n");
   fprintf(f, "   sqlite3_stmt *selectAll;\n");
   fprintf(f, "   sqlite3_stmt *deleteAll;\n");

   vecForEach(DBMember, member, strct->members,{
      if (member->mods&MOD(MODIFIER_SELECT)) {
         fprintf(f,
            "   sqlite3_stmt *selectBy%s;\n"
            "   sqlite3_stmt *deleteBy%s;\n",
            c_str(member->name), c_str(member->name));
      }
   });
   fprintf(f, "} DB%sStmts;\n\n", c_str(strct->name));
   });


   fprintf(f, "struct DB_%s{\n", c_str(fd->inputFileOnly));

   fprintf(f, "   DBBase base;\n");

   vecForEach(DBStruct, strct, fd->structs, {
      fprintf(f, "   DB%sStmts %sStmts;\n", c_str(strct->name), c_str(strct->name));
   });

   fprintf(f, "};\n\n");

   fprintf(f,
      "DB_%s *db_%sCreate(){\n"
      "   DB_%s *out = checkedCalloc(1, sizeof(DB_%s));\n"
      "   return out;\n"
      "}\n\n"
      , c_str(fd->inputFileOnly), c_str(fd->inputFileOnly), c_str(fd->inputFileOnly), c_str(fd->inputFileOnly));

   fprintf(f,
      "void db_%sDestroy(DB_%s *self){\n"
      "   dbDestroy((DBBase*)self);\n"
      , c_str(fd->inputFileOnly), c_str(fd->inputFileOnly));

   vecForEach(DBStruct, strct, fd->structs, {
      fprintf(f, "   db%sDestroyStatements(self);\n", c_str(strct->name));
   });

   fprintf(f,
      "   checkedFree(self);\n"
      "}\n\n");

   fprintf(f,
      "int db_%sCreateTables(DB_%s *self){\n"
      "   int result = 0;\n\n", c_str(fd->inputFileOnly), c_str(fd->inputFileOnly));

   vecForEach(DBStruct, strct, fd->structs, {
      fprintf(f, "   if((result = db%sCreateTable(self)) != DB_SUCCESS){ return result; }\n", c_str(strct->name));
   });

   fprintf(f, "\n   return DB_SUCCESS;\n}\n\n");
}

void createSource(FileData *data) {
   FILE *f = fopen(c_str(data->outputc), "w");

   if (!f) {
      printf("ERROR: Unable to create file %s.\n", c_str(data->outputc));
      return;
   }

   sourceWriteHeader(f, data);
   sourceCreateDB(f, data);

   vecForEach(DBStruct, strct, data->structs, {
      sourceCreateStruct(f, data, strct);
   });

   fclose(f);
}


static void _generateFiles(const char *file, DBGenLexer *lexer) {
   FileData data = { 0 };
   char namebuff[256] = { 0 };

   data.input = stringCreate(file);
   data.dir = stringGetDirectory(data.input);

   data.inputFileOnly = stringGetFilename(data.input);
   sprintf(namebuff, "DB%s", c_str(data.inputFileOnly));
   stringSet(data.inputFileOnly, namebuff);

   sprintf(namebuff, "%s.h", c_str(data.inputFileOnly));
   data.outputh = stringCreate(namebuff);

   sprintf(namebuff, "%s.c", c_str(data.inputFileOnly));
   data.outputc = stringCreate(namebuff);


   //prefixing the output with  directories sucks, should add a path combine funciton
   if (stringLen(data.dir) > 0) {
      String *dircpy = stringCopy(data.dir);
      stringConcatChar(dircpy, '/');
      stringConcat(dircpy, c_str(data.outputh));
      stringSet(data.outputh, c_str(dircpy));

      stringSet(dircpy, c_str(data.dir));
      stringConcatChar(dircpy, '/');
      stringConcat(dircpy, c_str(data.outputc));
      stringSet(data.outputc, c_str(dircpy));

      stringDestroy(dircpy);
   }

   printf(
      "* Output is:\n"
      "*    %s\n"
      "*    %s\n"
      ,
      c_str(data.outputh),
      c_str(data.outputc));

   data.structs = lexer->structs;

   createHeader(&data);
   createSource(&data);

   stringDestroy(data.input);
   stringDestroy(data.inputFileOnly);
   stringDestroy(data.dir);
   stringDestroy(data.outputh);
   stringDestroy(data.outputc);

}


void runDBGen(const char *file) {
   long fSize = 0;

   printf(
      "***********************************\n"
      "* DBGen: SQLite API Generator\n"
      "* Processing dbh file: %s\n\n", file);

   byte *buff = readFullFile(file, &fSize);
   Tokenizer *tokens = tokenizerCreate((StringStream) { .pos = buff, .last = buff + fSize });
   tokenizerAcceptFile(tokens);

   checkedFree(buff);

   DBGenLexer *lexer = dbGenLexerCreate((TokenStream) { .pos = vecBegin(Token)(tokens->tokens), .last = vecEnd(Token)(tokens->tokens) });
   lexerAcceptFile(lexer);

   _generateFiles(file, lexer);

   dbGenLexerDestroy(lexer);
   tokenizerDestroy(tokens);
}

static void _showHelp() {
   printf("Usage: dbgen [dbh file]\n");
}


int main(int argc, char *argv[]) {
   if (argc <= 1) {
      _showHelp();
      return;
   }

   runDBGen(argv[1]);
   printMemoryLeaks();
}