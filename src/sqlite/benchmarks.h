#ifndef __BENCHMARKS_H__
#define __BENCHMARKS_H__

#include "sqlite3.h"

#if 1 //original numbers
#  define BM_INSERTS                 1000
#  define BM_INSERTS_TRANSACTION     25000
#  define BM_INSERTS_INDEXED         25000

#  define BM_SELECTS_NOINDEXED       100
#  define BM_SELECTS_INDEXED         5000
#  define BM_SELECTS_STRCPY          100

#  define BM_UPDATES_INT_NOIDX_RANGE 1000
#  define BM_UPDATES_INT_INDX        25000
#  define BM_UPDATES_STR             25000
#else //custom numbers
#  define BM_INSERTS                 (100*1000)
#  define BM_INSERTS_TRANSACTION     (100*25000)
#  define BM_INSERTS_INDEXED         (100*25000)

#  define BM_SELECTS_NOINDEXED       (1*100)
#  define BM_SELECTS_INDEXED         (10*5000)
#  define BM_SELECTS_STRCPY          (100)

#  define BM_UPDATES_INT_NOIDX_RANGE (1*1000)
#  define BM_UPDATES_INT_INDX        (10*25000)
#  define BM_UPDATES_STR             (1*25000)
#endif

#define CHARS_PER_LINE          128
#define INT2WORDSMAX            1000000

#define USESPM                  1
#define NOTSPM                  0

int create_tables(struct sqlite3 * db, int n);
int insert_notransaction(struct sqlite3 * db, int tid, int rows);
int insert_transaction(struct sqlite3 * db, int tid, int rows);
int select_transaction(struct sqlite3 * db, int tid, int selects);
int select_strcmp(struct sqlite3 * db, int tid, int selects);
int update_range_transaction(struct sqlite3 * db, int tid, int selects);
int update_transaction(struct sqlite3 * db, int tid, int selects);
int update_str_transaction(struct sqlite3 * db, int tid, int selects);

void run_benchmarks(struct sqlite3 * db, const char * file);

void gen_random(char *s, const int len);
int insert_transaction_v2(struct sqlite3 * db, int rows);

//#define SQLCREATETBL "CREATE TABLE  tbl  ( id INTEGER NOT NULL, maddr INTEGER NOT NULL);"
#define STRSIZE (1024 - (8 + 8))
#define QUERYSIZE (STRSIZE + 128)
#define SQLCREATETBL "CREATE TABLE  tbl  (i integer, f float, s varchar(1025));"


void ope_create(sqlite3 * db);
void ope_insert(sqlite3 * db, int init, int norow, int transaction);
void ope_modify(sqlite3 * db, int init, int norow);
void ope_remove(sqlite3 * db, int init, int norow);
void ope_select(sqlite3 * db, int init, int norow);
int callback_print(void *NotUsed, int argc, char **argv, char **azColName);

#endif
