#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

#include "./handlers/clientHandlers.h"

#include "../app/config.h"
#include "../app/appFlags.h"

extern int errno;

int main()
{
    int sd;
    struct sockaddr_in server;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[client]Error at socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(HOST_IP);
    server.sin_port = htons(PORT);

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Error at connect().\n");
        return errno;
    }

    printf(
        "\n\033[31m\\__________________\033[0mWELCOME TO\033[31m__________________/\n"
        "\033[33m"
        "       ___        _     __  __                 \n"
        "      / _ \\ _   _(_)___|  \\/  | __ ___  __   \n"
        "     | | | | | | | |_  / |\\/| |/ _` \\ \\/ /  \n"
        "     | |_| | |_| | |/ /| |  | | (_| |>  <      \n"
        "      \\___\\_\\__,_|_/___|_|  |_|\\__,_/_/\\_\\\n"
        "\033[31m"
        " ______________________________________________\n"
        "/                                              \\\033[0m\n\n");

    int running = 1;
    int logged = 0;

    while (running)
    {
        if (logged == 0)
        {
            printf(
                "Commands:\n"
                "1 - Login\n"
                "2 - Register\n"
                "3 - Exit\n"
                "\n"
                "Input: "
            );
        }
        else
        {
            printf(
                "Commands:\n"
                "1 - Logout\n"
                "2 - Play\n"
                "3 - Exit\n"
                "\n"
                "Input: "
            );
        }

        char buffer[BUFFLEN];

        fflush(stdout);
        read(0, buffer, BUFFLEN);

        int command = atoi(buffer);

        if (logged == 0)
        {
            switch (command)
            {
            case 1:
            {
                if (handleLogin(sd) == 0)
                {
                    logged = 1;
                }
            }
            break;

            case 2:
            {
                handleRegister(sd);
            }
            break;

            case 3:
            {
                running = 0;
                int protocol = EXIT_COMMAND;

                if (write(sd, &protocol, sizeof(int)) <= 0)
                {
                    perror("[client]Error at write() to server.\n");
                    return errno;
                }
            }
            break;

            default:
            {
                printf("[client]Invalid command\n");
            }
            break;
            }
        }
        else
        {
            switch (command)
            {
            case 1:
            {
                printf("[client]Loged out\n");
                logged = 0;
            }
            break;

            case 2:
            {
                handlePlay(sd);
            }
            break;

            case 3:
            {
                running = 0;
                int protocol = EXIT_COMMAND;

                if (write(sd, &protocol, sizeof(int)) <= 0)
                {
                    perror("[client]Error at write() to server.\n");
                    return errno;
                }
            }
            break;

            default:
            {
                printf("[client]Invalid command\n");
            }
            break;
            }
        }
        
    }

    close(sd);
}
