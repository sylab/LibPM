#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <timediff.h>

#include <cont.h>
#include "int2words.h"
#include "sqlite3.h"
#include "benchmarks.h"

/*
 * These workloads were taken from
 *
 * http://www.sqlite.org/speed.html
 *
 */

int callback_print(void *NotUsed, int argc, char **argv, char **azColName)
{
        //NotUsed=0;
        //printf("%s:%s:%u: " "Aqui\n", __FILE__, __FUNCTION__, __LINE__);
        //fflush(stdout);
        int i;
        for(i=0; i<argc; i++){
                printf("%s = %s\n", azColName[i], argv[i] ? argv[i]: "NULL");
        }
        printf("\n");
        return 0;
}

int callback_nope(void *NotUsed, int argc, char **argv, char **azColName)
{
        //NotUsed=0;
        int i;
        for(i=0; i<argc; i++){
                //printf("%s = %s\n", azColName[i], argv[i] ? argv[i]: "NULL");
        }
        //printf("\n");
        return 0;
}




int create_tables(struct sqlite3 * db, int n)
{
        int i, rc;
        char *zErrMsg = 0;
        char buff[128];
        for (i=1; i<=n; i++) {
                sprintf(buff,
                        "CREATE TABLE t%i(a INTEGER, b INTEGER, c VARCHAR(150));", i);
                rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
                if( rc!=SQLITE_OK ){
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                        break;
                }
                //printf("%s\n", buff);
        }
        return rc;
}

int insert_notransaction(struct sqlite3 * db, int tid, int rows)
{
        int i, rc;
        char *zErrMsg = 0;
        srand(time(NULL));

        for (i=0; i<rows; i++) {
                char buff[256];
                int r = rand() % INT2WORDSMAX + 1;
                sprintf(buff, "INSERT INTO t%i VALUES (%i, %i, \"%s\");", tid, i, r, int2words(r));

                rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
                //printf("%s\n", buff);
                if( rc!=SQLITE_OK ){
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                        break;
                }
        }
        return rc;
}

int insert_transaction(struct sqlite3 * db, int tid, int rows)
{
        char * buff = NULL;
        char * line = NULL;
        char * ptr = NULL;
        char * begin = "BEGIN;";
        char * end = "COMMIT;\0";
        char * zErrMsg = 0;
        int i, rc;

        ptr = buff = malloc(CHARS_PER_LINE * (rows + 2));
        line = malloc(CHARS_PER_LINE);

        if (!buff || !line) goto end;

        memcpy(ptr, begin, strlen(begin));
        ptr += strlen(begin);

        srand(time(NULL));

        for (i=0; i<rows; i++) {
                int r = rand() % INT2WORDSMAX + 1;
                sprintf(line, "INSERT INTO t%i VALUES (%i, %i, \"%s\");", tid, i, r, int2words(r));
                memcpy(ptr, line, strlen(line));
                ptr += strlen(line);
        }

        memcpy(ptr, end, strlen(end) + 1);

        //printf("%s\n", buff);
        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                fprintf(stderr, buff, strlen(buff));
        }

end:
        if (buff) free(buff);
        if (line) free(line);
        return rc;
}

int select_transaction(struct sqlite3 * db, int tid, int selects)
{
        char * buff = NULL;
        char * line = NULL;
        char * ptr = NULL;
        char * begin = "BEGIN;";
        char * end = "COMMIT;\0";
        char * zErrMsg = 0;
        int i, rc;
        int min = 0, max = 1000, step = 100;

        ptr = buff = malloc(CHARS_PER_LINE * (selects + 2));
        line = malloc(CHARS_PER_LINE);

        if (!buff || !line) goto end;

        memcpy(ptr, begin, strlen(begin));
        ptr += strlen(begin);

        for (i=0; i<selects; i++) {
                sprintf(line, "SELECT COUNT(*), AVG(b) FROM t%i WHERE b>=%i AND b<%i;", tid, min, max);
                memcpy(ptr, line, strlen(line));
                ptr += strlen(line);
                min += step;
                max += step;
        }

        memcpy(ptr, end, strlen(end) + 1);

        //printf("%s\n", buff);
        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }

end:
        if (buff) free(buff);
        if (line) free(line);
        return rc;
}

int select_strcmp(struct sqlite3 * db, int tid, int selects)
{
        char * buff = NULL;
        char * line = NULL;
        char * ptr = NULL;
        char * begin = "BEGIN;";
        char * end = "COMMIT;\0";
        char * zErrMsg = 0;
        int i, rc;

        ptr = buff = malloc(CHARS_PER_LINE * (selects + 2));
        line = malloc(CHARS_PER_LINE);

        if (!buff || !line) goto end;

        memcpy(ptr, begin, strlen(begin));
        ptr += strlen(begin);

        for (i=0; i<selects; i++) {
                sprintf(line, "SELECT count(*), avg(b) FROM t%i WHERE c LIKE '%%%s%%';", tid, int2words(i));
                memcpy(ptr, line, strlen(line));
                ptr += strlen(line);
        }

        memcpy(ptr, end, strlen(end) + 1);

        //printf("%s\n", buff);
        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }

end:
        if (buff) free(buff);
        if (line) free(line);
        return rc;
}

int update_range_transaction(struct sqlite3 * db, int tid, int selects)
{
        char * buff = NULL;
        char * line = NULL;
        char * ptr = NULL;
        char * begin = "BEGIN;";
        char * end = "COMMIT;\0";
        char * zErrMsg = 0;
        int i, rc;
        int min = 0, max = 1000, step = 10;

        ptr = buff = malloc(CHARS_PER_LINE * (selects + 2));
        line = malloc(CHARS_PER_LINE);

        if (!buff || !line) goto end;

        memcpy(ptr, begin, strlen(begin));
        ptr += strlen(begin);

        for (i=0; i<selects; i++) {
                sprintf(line, "UPDATE t%i SET b=b*2 WHERE a>=%i AND a<%i;", tid, min, max);
                memcpy(ptr, line, strlen(line));
                ptr += strlen(line);
                min += step;
                max += step;
        }

        memcpy(ptr, end, strlen(end) + 1);

        //printf("%s\n", buff);
        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }

end:
        if (buff) free(buff);
        if (line) free(line);
        return rc;
}

int update_transaction(struct sqlite3 * db, int tid, int selects)
{
        char * buff = NULL;
        char * line = NULL;
        char * ptr = NULL;
        char * begin = "BEGIN;";
        char * end = "COMMIT;\0";
        char * zErrMsg = 0;
        int i, rc;

        ptr = buff = malloc(CHARS_PER_LINE * (selects + 2));
        line = malloc(CHARS_PER_LINE);

        if (!buff || !line) goto end;

        memcpy(ptr, begin, strlen(begin));
        ptr += strlen(begin);

        srand(time(NULL));
        for (i=0; i<selects; i++) {
                int r = rand() % INT2WORDSMAX + 1;
                sprintf(line, "UPDATE t%i SET b=%i WHERE a=%i;", tid, r, i);
                memcpy(ptr, line, strlen(line));
                ptr += strlen(line);
        }

        memcpy(ptr, end, strlen(end) + 1);

        //printf("%s\n", buff);
        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }

end:
        if (buff) free(buff);
        if (line) free(line);
        return rc;
}

int update_str_transaction(struct sqlite3 * db, int tid, int selects)
{
        char * buff = NULL;
        char * line = NULL;
        char * ptr = NULL;
        char * begin = "BEGIN;";
        char * end = "COMMIT;\0";
        char * zErrMsg = 0;
        int i, rc;

        ptr = buff = malloc(CHARS_PER_LINE * (selects + 2));
        line = malloc(CHARS_PER_LINE);

        if (!buff || !line) goto end;

        memcpy(ptr, begin, strlen(begin));
        ptr += strlen(begin);

        srand(time(NULL));
        for (i=0; i<selects; i++) {
                int r = rand() % INT2WORDSMAX + 1;
                sprintf(line, "UPDATE t%i SET c=\"%s\" WHERE a=%i;", tid, int2words(r), i);
                memcpy(ptr, line, strlen(line));
                ptr += strlen(line);
        }

        memcpy(ptr, end, strlen(end) + 1);

        //printf("%s\n", buff);
        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }

end:
        if (buff) free(buff);
        if (line) free(line);
        return rc;
}

void run_benchmarks(struct sqlite3 * db, const char * file) {
        int rc;
        struct timespec t0, t1;
        char * zErrMsg;
        rc = sqlite3_open(file, &db);
        if( rc )
        {
                fprintf(stderr, "Can't open database: %s\n",
                                sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(1);
        }

        rc = strcmp(file, ":memory:");

        clock_gettime(CLOCK_MONOTONIC, &t0);
        create_tables(db, 3);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T00 Creating tables: \t\t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 1: 1000 INSERTs
        clock_gettime(CLOCK_MONOTONIC, &t0);
        insert_notransaction(db, 1, BM_INSERTS);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T01 Inserting: \t\t\t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 2: 25000 INSERTs in a transaction
        clock_gettime(CLOCK_MONOTONIC, &t0);
        insert_transaction(db, 2, BM_INSERTS_TRANSACTION);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T02 Inserting Transaction: \t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 3: 25000 INSERTs into an indexed table
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rc = sqlite3_exec(db, "CREATE INDEX i3 on t3(c);", NULL, 0, &zErrMsg);
        if( rc )
        {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(1);
        }
        //clock_gettime(CLOCK_MONOTONIC, &t1);
        //printf("T03 Creating Index: \t%.3Lf\n", time_diff(t0, t1));

        //clock_gettime(CLOCK_MONOTONIC, &t0);
        insert_transaction(db, 3, BM_INSERTS_TRANSACTION);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T03 Inserting Transaction with index: \t\t%.3Lf\n", time_diff(t0, t1));

        //Test 4: 100 SELECTs without an index
        clock_gettime(CLOCK_MONOTONIC, &t0);
        select_transaction(db, 2, BM_SELECTS_NOINDEXED);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T04 Selects Transaction: \t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 5: 100 SELECTs on a string comparison
        //char arr[10];
        clock_gettime(CLOCK_MONOTONIC, &t0);
        select_strcmp(db, 2, BM_SELECTS_STRCPY);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T05 Selects Transaction on Str: \t\t%.3Lf\n", time_diff(t0, t1));

        //Test 6: Creating an index
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rc = sqlite3_exec(db,
                        "CREATE INDEX i2a on t2(a);CREATE INDEX i2b on t2(b);",
                        NULL, 0, &zErrMsg);
        if( rc )
        {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T06 Creating Indexes: \t\t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 7: 5000 SELECTs with an index
        clock_gettime(CLOCK_MONOTONIC, &t0);
        //NOTE: in the original test, the selects are not in a
        //transactions
        select_transaction(db, 2, BM_SELECTS_INDEXED);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T07 Selects with index: \t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 8: 1000 UPDATEs without an index
        clock_gettime(CLOCK_MONOTONIC, &t0);
        update_range_transaction(db, 1, BM_UPDATES_INT_NOIDX_RANGE);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T08 Updates without index: \t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 9: 25000 UPDATEs with an index
        clock_gettime(CLOCK_MONOTONIC, &t0);
        update_transaction(db, 2, BM_UPDATES_INT_INDX);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T09 Updates with index: \t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 10: 25000 text UPDATEs with an index
        clock_gettime(CLOCK_MONOTONIC, &t0);
        update_str_transaction(db, 2, BM_UPDATES_STR);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T10 Updates str with index: \t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 11: INSERTs from a SELECT
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rc = sqlite3_exec(db,
                        "BEGIN; INSERT INTO t1 SELECT b,a,c FROM t2; "
                        "INSERT INTO t2 SELECT b,a,c FROM t1; COMMIT;",
                        NULL, 0, &zErrMsg);
        if( rc )
        {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T11 Inserting from select: \t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 12: DELETE without an index
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rc = sqlite3_exec(db,
                        "DELETE FROM t2 WHERE c LIKE '%fifty%';",
                        NULL, 0, &zErrMsg);
        if( rc )
        {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T12 Deleting without index: \t\t\t%.3Lf\n", time_diff(t0, t1));

        //Test 13: DELETE with an index
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rc = sqlite3_exec(db,
                        "DELETE FROM t2 WHERE a>10 AND a<20000;",
                        NULL, 0, &zErrMsg);
        if( rc )
        {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T13 Deleting with index: \t\t\t%.3Lf\n", time_diff(t0, t1));
        
        //Test 14: A big INSERT after a big DELETE
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rc = sqlite3_exec(db,
                        "INSERT INTO t2 SELECT * FROM t1;",
                        NULL, 0, &zErrMsg);
        if( rc )
        {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T14 Big insert after big delete: \t\t%.3Lf\n", time_diff(t0, t1));

        //Test 15: A big DELETE followed by many small INSERTs
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rc = sqlite3_exec(db,
                        "DELETE FROM t1;",
                        NULL, 0, &zErrMsg);
        if( rc )
        {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(1);
        }
        insert_transaction(db, 1, BM_INSERTS_TRANSACTION);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T15 Big delete fallowed by small inserts: \t%.3Lf\n", time_diff(t0, t1));

        //Test 16: DROP TABLE
        clock_gettime(CLOCK_MONOTONIC, &t0);
        rc = sqlite3_exec(db,
                        "DROP TABLE t1; DROP TABLE t2; DROP TABLE t3;",
                        NULL, 0, &zErrMsg);
        if( rc )
        {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("T16 Drop tables: \t\t\t\t%.3Lf\n", time_diff(t0, t1));
}

/* ===================================================================== */

void gen_random(char *s, const int len)
{
        static const char alphanum[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";

        int i=0, off;
	off = rand();
        //for (i = 0; i < (len > 10 ? 10 : len); ++i) {
	        s[i] = alphanum[(off + i) % (sizeof(alphanum) - 1)];
                //}

        s[len] = 0;
}

int insert_transaction_v2(struct sqlite3 * db, int rows)
{
        char * buff = NULL;
        char * line = NULL;
        char * ptr = NULL;
        char * begin = "BEGIN;";
        char * end = "COMMIT;\0";
        char * zErrMsg = 0;
        int i, rc;
        float f;
        char s[STRSIZE];

        if (!rows) goto end;


        ptr = buff = malloc(QUERYSIZE * rows);
        line = malloc(QUERYSIZE);

        if (!buff || !line) goto end;

        memcpy(ptr, begin, strlen(begin));
        ptr += strlen(begin);

        srand(time(NULL));

        for (i=0; i<rows; i++) {
                f = (float) rand() / 2.02;
                gen_random(s, STRSIZE);
                //sprintf(line, "INSERT INTO tbl VALUES (%i, %f, '%s');", i, f, s);
                sprintf(line, "INSERT INTO tbl VALUES (%i, %f);", i, f);
                memcpy(ptr, line, strlen(line));
                ptr += strlen(line);
        }

        memcpy(ptr, end, strlen(end) + 1);

        //printf("%s\n", buff);
        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                fprintf(stderr, buff, strlen(buff));
        }

end:
        if (buff) free(buff);
        if (line) free(line);
        return rc;
}

/**************************************************/

void ope_create(sqlite3 * db)
{
        int rc;
        char * zErrMsg;

        rc = sqlite3_exec(db, "CREATE TABLE  tbl  (i integer, f float)", NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
        }
}

void ope_insert(sqlite3 * db, int init, int norow, int transaction)
{
        int continous = 1;
        int i, rc;
        char * zErrMsg = NULL;
        char s[STRSIZE];
        float f;
        char buff[1024];

        if (transaction > 1) {
                for (i=init; i<norow/transaction; i++) {
                        rc = insert_transaction_v2(db, transaction);
                        if( rc!=SQLITE_OK ){
                                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                                sqlite3_free(zErrMsg);
                        }
                }
                rc = insert_transaction_v2(db, norow % transaction);
                if( rc!=SQLITE_OK ){
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                }
        } else {
                for (i=0; i<norow; i++) {
                        f = (float) rand() / 1.02;
                        gen_random(s, STRSIZE);
                        //sprintf(buff, "INSERT INTO tbl VALUES (%i, %f, '%s')", i, f, s);
                        sprintf(buff, "INSERT INTO tbl VALUES (%i, %f)", i, f);

                        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
                        if( rc!=SQLITE_OK ){
                                fprintf(stderr, "SQL error after insert: %s\n", zErrMsg);
                                sqlite3_free(zErrMsg);
                                /*return -1;*/
                        }
                }
        }
}

void ope_modify(sqlite3 * db, int init, int norow)
{
        char buff[1024];
        char * zErrMsg = NULL;
        int rc;

        sprintf(buff, "update tbl set f='1.1' WHERE i>=%i AND i<%i;", init, norow);
        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error after modifying: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                /*return -1;*/
        }
}

void ope_remove(sqlite3 * db, int init, int norow)
{
        char buff[1024];
        char * zErrMsg = NULL;
        int rc;

        sprintf(buff, "delete from tbl WHERE i>=%i AND i<%i;", init, norow);
        rc = sqlite3_exec(db, buff, NULL, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error after removing: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                /*return -1;*/
        }
}

void ope_select(sqlite3 * db, int init, int norow)
{
        char buff[1024];
        char * zErrMsg = NULL;
        int rc;

        sprintf(buff, "select * from tbl WHERE i>=%i AND i<%i;", init, norow);
        rc = sqlite3_exec(db, buff, callback_nope, 0, &zErrMsg);
        if( rc!=SQLITE_OK ){
                fprintf(stderr, "SQL error after removing: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                /*return -1;*/
        }
}
