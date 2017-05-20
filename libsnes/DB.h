#pragma once

#include "libutils/String.h"

typedef struct DB_t DB;

#define DB_SUCCESS 0
#define DB_FAILURE 1

//new shit
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

typedef struct DBBase {
   sqlite3 *conn;
   String *dbPath, *err;
   boolean open;
}DBBase;

int dbConnect(DBBase *self, const char *filename, boolean create);
int dbDisconnect(DBBase *self);

boolean _dbIsConnected(DBBase *self);
#define dbIsConnected(db) _dbIsConnected((DBBase*)db)

void dbDestroy(DBBase *self);//this does not call free on self!!
const char *_dbGetError(DBBase *self);
void _dbClearError(DBBase *self);
#define dbGetError(db) _dbGetError((DBBase*)db)
#define dbClearError(db) _dbClearError((DBBase*)db)

int _dbBeginTransaction(DBBase *self);
int _dbRollbackTransaction(DBBase *self);
int _dbCommitTransaction(DBBase *self);
#define dbBeginTransaction(db) _dbBeginTransaction((DBBase*)db)
#define dbRollbackTransaction(db) _dbRollbackTransaction((DBBase*)db)
#define dbCommitTransaction(db) _dbCommitTransaction((DBBase*)db)

int _dbExecute(DBBase *self, const char *cmd);
#define dbExecute(db, cmd) _dbExecute((DBBase*)db, cmd)

//if *out is null, it will create and preapre a new statement there
//otherwise it will call reset on it
int dbPrepareStatement(DBBase *self, sqlite3_stmt **out, const char *stmt);