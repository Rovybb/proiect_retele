#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>

#include "clientHandlers.h"
#include "../../app/config.h"
#include "../../app/appFlags.h"

extern int errno;

int handleLogin(int sd)
{
    int intBuffer, accExists;
    int protocol = LOGIN_COMMAND;

    if (write(sd, &protocol, sizeof(int)) <= 0)
    {
        perror("[client]Error at write() to server.\n");
        return errno;
    }

    printf("\n\033[31m\\____________________\033[0mLOGIN\033[31m____________________/\n\033[0m\n");

    do
    {
        printf("Username: ");
        fflush(stdout);
        char username[MAX_USERNAME_LENGTH];
        bzero(username, MAX_USERNAME_LENGTH);
        read(0, username, MAX_USERNAME_LENGTH);

        if (username[strlen(username) - 1] == '\n')
        {
            username[strlen(username) - 1] = '\0';
        }

        if (write(sd, username, strlen(username)) <= 0)
        {
            perror("[client]Error at write() to server.\n");
            return errno;
        }

        if (read(sd, &intBuffer, sizeof(int)) <= 0)
        {
            perror("[client]Error at read() from server.\n");
            return errno;
        }

        if (intBuffer == ACCOUNT_DOESNT_EXIST)
        {
            printf("[client]Username not found\n");
            fflush(stdout);
        }

        accExists = intBuffer;

        if (read(sd, &intBuffer, sizeof(int)) <= 0)
        {
            perror("[client]Error at read() from server.\n");
            return errno;
        }

        if (intBuffer == USER_LOGED_IN)
        {
            printf("[client]User already loged in\n");
            fflush(stdout);
        }
    } while (accExists == ACCOUNT_DOESNT_EXIST || intBuffer == USER_LOGED_IN);

    do
    {
        printf("Password: ");
        fflush(stdout);
        char password[MAX_PASSWORD_LENGTH];
        bzero(password, MAX_PASSWORD_LENGTH);
        read(0, password, MAX_PASSWORD_LENGTH);

        if (password[strlen(password) - 1] == '\n')
        {
            password[strlen(password) - 1] = '\0';
        }

        if (write(sd, password, strlen(password)) <= 0)
        {
            perror("[client]Error at write() to server.\n");
            return errno;
        }

        if (read(sd, &intBuffer, sizeof(int)) <= 0)
        {
            perror("[client]Error at read() from server.\n");
            return errno;
        }

        if (intBuffer == PASSWORD_DOESNT_EXIST)
        {
            printf("[client]Wrong password\n");
            fflush(stdout);
        }
        else if (intBuffer == PASSWORD_EXISTS)
        {
            if (read(sd, &intBuffer, sizeof(int)) <= 0)
            {
                perror("[client]Error at read() from server.\n");
                return errno;
            }

            if (intBuffer == PASSWORDS_DONT_MATCH)
            {
                printf("[client]Wrong password\n");
                fflush(stdout);
            }
        }

    } while (intBuffer == PASSWORD_DOESNT_EXIST || intBuffer == PASSWORDS_DONT_MATCH);

    if (read(sd, &intBuffer, sizeof(int)) <= 0)
    {
        perror("[client]Error at read() from server.\n");
        return errno;
    }

    if (intBuffer == LOGIN_SUCCESS)
    {
        printf("[client]Login successful\n");
        fflush(stdout);
    }
    else if (intBuffer == LOGIN_FAIL)
    {
        printf("[client]Login failed\n");
        fflush(stdout);
    }

    return 0;
}

int handleRegister(int sd)
{
    int intBuffer;
    int protocol = REGISTER_COMMAND;

    if (write(sd, &protocol, sizeof(int)) <= 0)
    {
        perror("[client]Error at write() to server.\n");
        return errno;
    }

    printf("\n\033[31m\\___________________\033[0mREGISTER\033[31m___________________/\n\033[0m\n");

    do
    {
        printf("Username: ");
        fflush(stdout);
        char username[MAX_USERNAME_LENGTH];
        bzero(username, MAX_USERNAME_LENGTH);
        read(0, username, MAX_USERNAME_LENGTH);

        if (username[strlen(username) - 1] == '\n')
        {
            username[strlen(username) - 1] = '\0';
        }

        if (write(sd, username, strlen(username)) <= 0)
        {
            perror("[client]Error at write() to server.\n");
            return errno;
        }

        if (read(sd, &intBuffer, sizeof(int)) <= 0)
        {
            perror("[client]Error at read() ehehe from server.\n");
            return errno;
        }

        if (intBuffer == USERNAME_TAKEN)
        {
            printf("[client]Username already exists\n");
            fflush(stdout);
        }
    } while (intBuffer == USERNAME_TAKEN);

    printf("Password: ");
    fflush(stdout);
    char password[MAX_PASSWORD_LENGTH];
    bzero(password, MAX_PASSWORD_LENGTH);
    read(0, password, MAX_PASSWORD_LENGTH);

    if (password[strlen(password) - 1] == '\n')
    {
        password[strlen(password) - 1] = '\0';
    }

    if (write(sd, password, strlen(password)) <= 0)
    {
        perror("[client]Error at write() to server.\n");
        return errno;
    }

    do
    {
        printf("Repeat password: ");
        fflush(stdout);
        char repeatPassword[MAX_PASSWORD_LENGTH];
        bzero(repeatPassword, MAX_PASSWORD_LENGTH);
        read(0, repeatPassword, MAX_PASSWORD_LENGTH);

        if (repeatPassword[strlen(repeatPassword) - 1] == '\n')
        {
            repeatPassword[strlen(repeatPassword) - 1] = '\0';
        }

        if (write(sd, repeatPassword, strlen(repeatPassword)) <= 0)
        {
            perror("[client]Error at write() to server.\n");
            return errno;
        }

        if (read(sd, &intBuffer, sizeof(int)) <= 0)
        {
            perror("[client]Error at read() from server.\n");
            return errno;
        }

        if (intBuffer == PASSWORDS_DONT_MATCH)
        {
            printf("[client]Passwords don't match\n");
            fflush(stdout);
        }
    } while (intBuffer == PASSWORDS_DONT_MATCH);

    if (read(sd, &intBuffer, sizeof(int)) <= 0)
    {
        perror("[client]Error at read() from server.\n");
        return errno;
    }

    if (intBuffer == REGISTER_SUCCESS)
    {
        printf("[client]Register successful\n");
        fflush(stdout);
    }
    else if (intBuffer == REGISTER_FAIL)
    {
        printf("[client]Register failed\n");
        fflush(stdout);
    }

    return 0;
}

int handleLogout(int sd)
{
    int intBuffer;
    int protocol = LOGOUT_COMMAND;

    if (write(sd, &protocol, sizeof(int)) <= 0)
    {
        perror("[client]Error at write() to server.\n");
        return errno;
    }

    if (read(sd, &intBuffer, sizeof(int)) <= 0)
    {
        perror("[client]Error at read() from server.\n");
        return errno;
    }

    if (intBuffer == LOGOUT_SUCCESS)
    {
        printf("[client]Logout successful\n");
        fflush(stdout);
    }
    else if (intBuffer == LOGOUT_FAIL)
    {
        printf("[client]Logout failed\n");
        fflush(stdout);
    }

    return 0;
}

int handlePlay(int sd)
{
    int intBuffer;
    int protocol = ENTER_GAME_COMMAND;

    if (write(sd, &protocol, sizeof(int)) <= 0)
    {
        perror("[client]Error at write() to server.\n");
        return errno;
    }

    printf("\n\033[31m\\____________________\033[0mQUIZZ\033[31m____________________/\n\033[0m\n");

    for (int q = 0; q < 5; q++) ////////////////////////
    {
        char userAnswer[5];
        char question[256];
        bzero(question, 256);

        if (read(sd, question, 256) <= 0)
        {
            perror("[client]Error at read() from serveeeeeer.\n");
            return errno;
        }

        printf("%s\n", question);
        fflush(stdout);

        printf("Answer: ");
        fflush(stdout);

        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(0, &readfds);

        int selectResult = select(1, &readfds, NULL, NULL, &timeout);

        if (selectResult == -1)
        {
            perror("[client]Error at select() from stdin.\n");
            return errno;
        }
        else if (selectResult == 0)
        {
            printf("\n[client]Time's up!\n");
            fflush(stdout);
            strcpy(userAnswer, "exit");
        }
        else if(FD_ISSET(0, &readfds))
        {
            bzero(userAnswer, 5);
            read(0, userAnswer, 5);
        }

        if (userAnswer[strlen(userAnswer) - 1] == '\n')
        {
            userAnswer[strlen(userAnswer) - 1] = '\0';
        }

        if (write(sd, userAnswer, 5) <= 0)
        {
            perror("[client]Error at write() to server.\n");
            return errno;
        }

        if(strcmp(userAnswer, "exit") == 0)
        {
            if(read(sd, &intBuffer, sizeof(int)) <= 0)
            {
                perror("[client]Error at read() from server.\n");
                return errno;
            }

            if(intBuffer == ENTER_GAME_SUCCESS)
            {
                printf("[client]Game finished\n");
                fflush(stdout);
            }

            return 3;
        }
    }

    char leaderboard[1024];
    if (read(sd, leaderboard, 1024) <= 0)
    {
        perror("[client]Error at read() from server.\n");
        return errno;
    }

    printf("\n\033[31m\\____________________\033[0mLEADERBOARD\033[31m____________________/\n\033[0m\n");
    printf("%s\n", leaderboard);
    fflush(stdout);

    if(read(sd, &intBuffer, sizeof(int)) <= 0)
    {
        perror("[client]Error at read() from server.\n");
        return errno;
    }

    if(intBuffer == ENTER_GAME_SUCCESS)
    {
        printf("[client]Game finished\n");
        fflush(stdout);
    }

    return 0;
}