#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>
#include "sqlite3.h"

#include "benchmarks.h"

const char *program_name;

void print_usage(FILE* stream, int exit_code)
{
    fprintf(stream, "Usage: %s options\n", program_name);
    fprintf(stream,
            "  -h  --help             Display usage.\n"
            "  -d  --database file    Write to file.\n"
            "  -i  --input file       Read from file.\n"
            "  -b  --benchmark        Run benchmark.\n"
           );
    exit(exit_code);
}

int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i;
    for(i=0; i<argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i]: "NULL");
    }
    printf("\n");
    return 0;
}

void exec_sqlfile(char *file, sqlite3 *db)
{
    FILE *fp;
    char *line = NULL;
    char *zErrMsg = 0;
    size_t len = 0;
    ssize_t read;
    int rc;

    fp = fopen(file, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {

        if ( line[0] == '-' && line[1] == '-' )
            continue;

        printf("SQL Stmt: %s\n", line);

        rc = sqlite3_exec(db, line, callback, 0, &zErrMsg);

        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);

            if (zErrMsg)
                free(zErrMsg);

            sqlite3_close(db);
        }
    }

    if (line)
        free(line);
}

int main(int argc, const char *argv[])
{
    program_name = argv[0];

    int rc;
    char *input_file = NULL;
    char *db_file = NULL;
    int db_in_memory = 1;
    int benchmark_enabled = 0;

    int next_option;
    const char * const short_options = "hd:i:b";
    const struct option long_options[] = {
        { "help",     0, NULL, 'h' },
        { "database", 1, NULL, 'd' },
        { "input",    1, NULL, 'i' },
        { "benchmark",1, NULL, 'b' },
        { NULL,       0, NULL,  0  }
    };

    do {
        next_option = getopt_long(argc, argv, short_options, long_options, NULL);

        switch (next_option) {
            case 'h': print_usage(stdout, 0);               break;
            case 'i': input_file = optarg;                  break;
            case 'd': db_file = optarg; db_in_memory = 0;   break;
            case 'b': benchmark_enabled = 1;                break;
            case -1:                                        break;
            default: abort();
        }

    } while (next_option != -1);

    if (!input_file && !benchmark_enabled)
        print_usage(stderr, 1);

    char *file = db_in_memory ? ":memory:" : db_file;

    sqlite3 *db = NULL;
    if (benchmark_enabled) {
        run_benchmarks(&db, file);
    } else {
        rc = sqlite3_open(file, &db);
        if (rc)
        {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            exit(1);
        }

        exec_sqlfile(input_file, db);

        sqlite3_close(db);
    }

    return 0;
}
