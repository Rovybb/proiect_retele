#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>

#include "./handlers/serverHandlers.h"
#include "./game/gameHandlers.h"

#include "../app/config.h"
#include "../app/appFlags.h"

extern int errno;

typedef struct thData
{
    int idThread;
    int cl;
} thData;

static void *treat(void *);
void response(void *);

int main()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sd;
    pthread_t th[100];
    int i = 1;

    pthread_create(&th[0], NULL, &gameHandler, NULL);

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Error at socket().\n");
        return errno;
    }

    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Error at bind().\n");
        return errno;
    }

    if (listen(sd, 2) == -1)
    {
        perror("[server]Error at listen().\n");
        return errno;
    }

    thData *gameTd;

    while (1)
    {
        int client;
        thData *td;
        int length = sizeof(from);

        printf("[server]Waiting at clients at %d\n", PORT);
        fflush(stdout);

        if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
        {
            perror("[server]Error at accept().\n");
            continue;
        }

        td = (struct thData *)malloc(sizeof(struct thData));
        td->idThread = i++;
        td->cl = client;

        pthread_create(&th[i], NULL, &treat, td);
    }
};

static void *treat(void *arg)
{
    struct thData tdL;
    tdL = *((struct thData *)arg);
    pthread_detach(pthread_self());
    response((struct thData *)arg);
    close((intptr_t)arg);
    return (NULL);
};

void response(void *arg)
{
    int protocol = 0;
    struct thData tdL;
    tdL = *((struct thData *)arg);
    char currentUser[MAX_USERNAME_LENGTH];

    int running = 1;
    while (running)
    {
        if (read(tdL.cl, &protocol, sizeof(int)) <= 0)
        {
            printf("[Thread %d]Error at read() from client\n", tdL.idThread);
        }

        switch (protocol)
        {
        case LOGIN_COMMAND:
        {
            if (handleLoginServer(tdL.cl, tdL.idThread, currentUser) == 0)
            {
                int flag = LOGIN_SUCCESS;
                write(tdL.cl, &flag, sizeof(int));
            }
        }
        break;

        case ENTER_GAME_COMMAND:
        {
            int res;
            if ((res = handlePlayServer(tdL.cl, tdL.idThread, currentUser)) == 0)
            {
                int flag = ENTER_GAME_SUCCESS;
                write(tdL.cl, &flag, sizeof(int));
            }
            else if (res == 3)
            {
                printf("[Thread %d]Client disconnected\n", tdL.idThread);
                fflush(stdout);
                running = 0;
            }
        }
        break;

        case REGISTER_COMMAND:
        {
            if (handleRegisterServer(tdL.cl, tdL.idThread) == 0)
            {
                int flag = REGISTER_SUCCESS;
                write(tdL.cl, &flag, sizeof(int));
            }
        }
        break;

        case LOGOUT_COMMAND:
        {
            if (handleLogoutServer(tdL.cl, tdL.idThread) == 0)
            {
                int flag = LOGOUT_SUCCESS;
                write(tdL.cl, &flag, sizeof(int));
            }
        }

        case EXIT_COMMAND:
        {
            printf("[Thread %d]Client disconnected\n", tdL.idThread);
            fflush(stdout);
            running = 0;
        }

        default:
            break;
        }
    }
}
