#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sqlite3.h>

#include "serverHandlers.h"

#include "../../app/config.h"
#include "../../app/appFlags.h"

extern int errno;

int handleLoginServer(int sd, int thid, char *currentUser)
{
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];

    if (sqlite3_open(ACCDB_PATH, &db) != SQLITE_OK)
    {
        printf("[Thread %d]Error opening database: %s\n", thid, sqlite3_errmsg(db));
        fflush(stdout);
        sqlite3_close(db);
        return -1;
    }

    int exists = 0;

    while (exists == 0)
    {
        bzero(username, MAX_USERNAME_LENGTH);
        read(sd, username, MAX_USERNAME_LENGTH);

        char *sql = "SELECT username FROM accounts WHERE username = ?;";

        if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK)
        {
            printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
            fflush(stdout);
            sqlite3_close(db);
            return -1;
        }
        else
        {
            sqlite3_bind_text(res, 1, username, strlen(username), 0);
        }

        int step = sqlite3_step(res);

        const unsigned char *usr = sqlite3_column_text(res, 0);
        int logedin = sqlite3_column_int(res, 2);

        if (step == SQLITE_ROW)
        {
            int flag = ACCOUNT_EXISTS;
            write(sd, &flag, sizeof(int));
            exists = 1;
        }
        else if (step == SQLITE_DONE)
        {
            printf("[Thread %d]Username doesn't exist\n", thid);
            fflush(stdout);
            int flag = ACCOUNT_DOESNT_EXIST;
            write(sd, &flag, sizeof(int));
        }
        else
        {
            printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
            fflush(stdout);
            sqlite3_close(db);
            return -1;
        }

        if (logedin == 1)
        {
            printf("[Thread %d]User already loged in\n", thid);
            fflush(stdout);
            int flag = USER_LOGED_IN;
            write(sd, &flag, sizeof(int));
            exists = 0;
        }
        else
        {
            int flag = USER_NOT_LOGED_IN;
            write(sd, &flag, sizeof(int));
        }

        sqlite3_finalize(res);
    }

    int passwordMatches = 0;

    while (passwordMatches == 0)
    {
        bzero(password, MAX_PASSWORD_LENGTH);
        read(sd, password, MAX_PASSWORD_LENGTH);

        char *sql = "SELECT password FROM accounts WHERE username = ?;";

        if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK)
        {
            printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
            fflush(stdout);
            sqlite3_close(db);
            return -1;
        }
        else
        {
            sqlite3_bind_text(res, 1, username, strlen(username), 0);
        }

        int step = sqlite3_step(res);

        if (step == SQLITE_ROW)
        {
            int flag = PASSWORD_EXISTS;
            write(sd, &flag, sizeof(int));

            const char *pass = sqlite3_column_text(res, 0);

            if (strcmp(password, pass) == 0)
            {
                int flag = PASSWORDS_MATCH;
                write(sd, &flag, sizeof(int));
                passwordMatches = 1;
            }
            else
            {
                printf("[Thread %d]Passwords don't match\n", thid);
                fflush(stdout);
                int flag = PASSWORDS_DONT_MATCH;
                write(sd, &flag, sizeof(int));
            }
        }
        else if (step == SQLITE_DONE)
        {
            printf("[Thread %d]Password doesn't exist\n", thid);
            fflush(stdout);
            int flag = PASSWORD_DOESNT_EXIST;
            write(sd, &flag, sizeof(int));
        }
        else
        {
            printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
            fflush(stdout);
            sqlite3_close(db);
            return -1;
        }

        sqlite3_finalize(res);
    }

    char statement[BUFFLEN];
    bzero(statement, BUFFLEN);
    sprintf(statement, "UPDATE accounts SET logedin = '%d' WHERE username = '%s';", 1, username);
    char sql[256];  
    bzero(sql, 256);
    strncpy(sql, statement, strlen(statement));

    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK)
    {
        printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
        fflush(stdout);
        sqlite3_close(db);
        return -1;
    }

    printf("[Thread %d]Login successful\n", thid);

    sqlite3_close(db);

    bzero(currentUser, MAX_USERNAME_LENGTH);
    strncpy(currentUser, username, strlen(username));

    return 0;
}

int handleRegisterServer(int sd, int thid)
{
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;

    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    char repeatPassword[MAX_PASSWORD_LENGTH];

    if (sqlite3_open(ACCDB_PATH, &db) != SQLITE_OK)
    {
        printf("[Thread %d]Error opening database: %s\n", thid, sqlite3_errmsg(db));
        fflush(stdout);
        sqlite3_close(db);
        return -1;
    }

    int different = 1;

    while (different)
    {
        bzero(username, MAX_USERNAME_LENGTH);
        read(sd, username, MAX_USERNAME_LENGTH);

        char *sql = "SELECT username FROM accounts WHERE username = ?;";

        if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK)
        {
            printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
            fflush(stdout);
            sqlite3_close(db);
            return -1;
        }
        else
        {
            sqlite3_bind_text(res, 1, username, strlen(username), 0);
        }

        int step = sqlite3_step(res);

        if (step == SQLITE_ROW)
        {
            printf("[Thread %d]Username already exists\n", thid);
            fflush(stdout);
            int flag = USERNAME_TAKEN;
            write(sd, &flag, sizeof(int));
        }
        else if (step == SQLITE_DONE)
        {
            int flag = USERNAME_AVAILABLE;
            write(sd, &flag, sizeof(int));
            different = 0;
        }
        else
        {
            printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
            fflush(stdout);
            sqlite3_close(db);
            return -1;
        }

        sqlite3_finalize(res);
    }

    bzero(password, MAX_PASSWORD_LENGTH);
    read(sd, password, MAX_PASSWORD_LENGTH);

    different = 1;
    while (different)
    {
        bzero(repeatPassword, MAX_PASSWORD_LENGTH);
        read(sd, repeatPassword, MAX_PASSWORD_LENGTH);

        int flag;
        if (strcmp(password, repeatPassword) == 0)
        {
            flag = PASSWORDS_MATCH;
            write(sd, &flag, sizeof(int));
            different = 0;
        }
        else
        {
            printf("[Thread %d]Passwords don't match\n", thid);
            fflush(stdout);
            flag = PASSWORDS_DONT_MATCH;
            write(sd, &flag, sizeof(int));
        }
    }

    char statement[BUFFLEN];
    bzero(statement, BUFFLEN);
    sprintf(statement, "INSERT INTO accounts (username, password) VALUES ('%s', '%s');", username, password);
    char sql[256];
    bzero(sql, 256);
    strncpy(sql, statement, strlen(statement));

    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK)
    {
        printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
        fflush(stdout);
        sqlite3_close(db);
        return -1;
    }

    printf("[Thread %d]Account created\n", thid);

    sqlite3_close(db);

    return 0;
}

int handleLogoutServer(int sd, int thid)
{
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;

    if (sqlite3_open(ACCDB_PATH, &db) != SQLITE_OK)
    {
        printf("[Thread %d]Error opening database: %s\n", thid, sqlite3_errmsg(db));
        fflush(stdout);
        sqlite3_close(db);
        return -1;
    }

    char statement[BUFFLEN];
    bzero(statement, BUFFLEN);
    sprintf(statement, "INSERT INTO accounts (logedin) VALUES ('%d');", 0);
    char sql[256];
    bzero(sql, 256);
    strncpy(sql, statement, strlen(statement));

    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK)
    {
        printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
        fflush(stdout);
        sqlite3_close(db);
        return -1;
    }

    printf("[Thread %d]Loged out successful\n", thid);

    sqlite3_close(db);

    return 0;
}