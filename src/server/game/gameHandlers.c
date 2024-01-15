#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>
#include <sqlite3.h>
#include <signal.h>
#include <semaphore.h>

#include "gameHandlers.h"

#include "../../app/config.h"
#include "../../app/appFlags.h"

typedef struct thData
{
    int idThread;
    int cl;
    time_t remaningTime;
    time_t endTime;
} thData;

typedef struct queueData
{
    int sd;
    struct sockaddr_in from;
} queueData;

typedef struct scoreData
{
    int score;
    char playerName[MAX_USERNAME_LENGTH];
} scoreData;

const int queueTime = 10;
int nrOfPlayers = 0;

sem_t leaderboardSem;

int i_game = 0;
pthread_t th_game[100];
pthread_t queue_th;

scoreData playerScores[100];

static void *queueHandler(void *);
static void *gameTreat(void *);
void queueCancelHandler(int);

void *gameHandler(void *arg)
{
    struct sockaddr_in gameServer;
    struct sockaddr_in from;
    int sd;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[game]Error at socket().\n");
        return (NULL);
    }

    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&gameServer, sizeof(gameServer));
    bzero(&from, sizeof(from));

    gameServer.sin_family = AF_INET;
    gameServer.sin_addr.s_addr = htonl(INADDR_ANY);
    gameServer.sin_port = htons(GAME_PORT);

    if (bind(sd, (struct sockaddr *)&gameServer, sizeof(struct sockaddr)) == -1)
    {
        perror("[game]Error at bind().\n");
        return (NULL);
    }

    if (listen(sd, 2) == -1)
    {
        perror("[game]Error at listen().\n");
        return (NULL);
    }

    while (1)
    {
        i_game = 0;

        queueData *queueData = (struct queueData *)malloc(sizeof(struct queueData));
        queueData->sd = sd;
        queueData->from = from;

        pthread_create(&queue_th, NULL, &queueHandler, queueData);

        printf("[game]A game has started, queue time: %d.\n", queueTime);

        signal(SIGALRM, queueCancelHandler);
        alarm(queueTime);

        pthread_join(queue_th, NULL);

        if (nrOfPlayers == 0)
        {
            printf("[game]Not enough players.\n");
            fflush(stdout);
        }
        else if (nrOfPlayers > 0)
        {
            sem_init(&leaderboardSem, 0, 0);

            memset(playerScores, 0, sizeof(playerScores));

            for (int ind = 0; ind < nrOfPlayers; ind++)
            {
                void *playerScore;
                playerScore = (struct scoreData *)malloc(sizeof(struct scoreData));
                if (pthread_join(th_game[ind], &playerScore) != 0)
                {
                    printf("[game]Error at pthread_join().\n");
                    fflush(stdout);
                }
                if (strcmp(((struct scoreData *)playerScore)->playerName, "exit") != 0)
                {
                    playerScores[ind] = *((struct scoreData *)playerScore);
                }
                free(playerScore);
            }

            printf("[game]Game ended.\n");

            int sorted = 0;
            while (sorted == 0)
            {
                sorted = 1;

                for (int ind = 0; ind < nrOfPlayers - 1; ind++)
                {
                    if (playerScores[ind].score < playerScores[ind + 1].score)
                    {
                        scoreData aux = playerScores[ind];
                        playerScores[ind] = playerScores[ind + 1];
                        playerScores[ind + 1] = aux;
                        sorted = 0;
                    }
                }
            }

            for (int i = 0; i < nrOfPlayers; i++)
            {
                sem_post(&leaderboardSem);
            }
            sem_destroy(&leaderboardSem);
        }
    }
}

void queueCancelHandler(int signum)
{
    printf("[game]Queue timer expired.\n");
    pthread_cancel(queue_th);
}

static void *queueHandler(void *arg)
{
    nrOfPlayers = 0;

    time_t startTime, endTime;
    startTime = time(NULL);
    endTime = startTime + queueTime;

    struct queueData queueData;
    queueData = *((struct queueData *)arg);

    do
    {
        int client;
        thData *td;

        int length = sizeof(queueData.from);

        if ((client = accept(queueData.sd, (struct sockaddr *)&queueData.from, &length)) < 0)
        {
            perror("[game]Error at accept().\n");
            continue;
        }

        startTime = time(NULL);

        td = (struct thData *)malloc(sizeof(struct thData));
        td->idThread = i_game;
        td->cl = client;
        td->remaningTime = endTime - startTime;
        td->endTime = endTime;

        nrOfPlayers++;
        pthread_create(&th_game[i_game], NULL, &gameTreat, td);
        i_game++;
        printf("[game]New player connected.\n");
    } while (startTime < endTime);

    return (NULL);
};

static void *gameTreat(void *arg)
{
    struct thData tdL;
    tdL = *((struct thData *)arg);

    int correctAnswers = 0;
    char username[MAX_USERNAME_LENGTH];

    bzero(username, MAX_USERNAME_LENGTH);
    if (read(tdL.cl, username, MAX_USERNAME_LENGTH) <= 0)
    {
        printf("[Player %d]Error at read() from client.\n", tdL.idThread);
        fflush(stdout);
        return NULL;
    }

    scoreData *playerScore;
    playerScore = (struct scoreData *)malloc(sizeof(struct scoreData));
    playerScore->score = 0;
    strcpy(playerScore->playerName, username);

    printf("[Player %d]Waiting for game start. Time: %d\n", tdL.idThread, (int)tdL.remaningTime);

    sleep((unsigned int)tdL.remaningTime);

    printf("[Player %d]Game started.\n", tdL.idThread);
    fflush(stdout);

    int flag = GAME_START;
    if (write(tdL.cl, &flag, sizeof(int)) <= 0)
    {
        printf("[Player %d]Error at write() to client.\n", tdL.idThread);
        fflush(stdout);
        return NULL;
    }

    for (int q = 0; q < 5; q++)
    {
        int userAnswer;
        if (read(tdL.cl, &userAnswer, sizeof(int)) <= 0)
        {
            printf("[Player %d]Error at read() from client.\n", tdL.idThread);
            fflush(stdout);
            return playerScore;
        }

        switch (userAnswer)
        {
        case EXIT_COMMAND:
        {
            int flag = EXIT_COMMAND;
            if (write(tdL.cl, &flag, sizeof(int)) <= 0)
            {
                printf("[Player %d]Error at write() to client.\n", tdL.idThread);
                fflush(stdout);
                return (void *)playerScore;
            }

            strcpy(playerScore->playerName, "exit");
            close((intptr_t)arg);
            return (void *)playerScore;
        }
        break;

        case 10:
        {
            correctAnswers++;
            int flag = GAME_CONFIRM_SUCCESS;

            if (write(tdL.cl, &flag, sizeof(int)) <= 0)
            {
                printf("[Player %d]Error at write() to client.\n", tdL.idThread);
                fflush(stdout);
                return (void *)playerScore;
            }
        }
        break;

        case 0:
        {
            int flag = GAME_CONFIRM_SUCCESS;

            if (write(tdL.cl, &flag, sizeof(int)) <= 0)
            {
                printf("[Player %d]Error at write() to client.\n", tdL.idThread);
                fflush(stdout);
                return (void *)playerScore;
            }
        }
        break;

        default:
            break;
        }
    }

    playerScore->score = correctAnswers * 20;
    close((intptr_t)arg);
    return (void *)playerScore;
}

int handlePlayServer(int socket_fd, int thid, char *currentUser)
{
    int sd;
    struct sockaddr_in server;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("[Thread %d]Error at socket().\n", thid);
        fflush(stdout);
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(HOST_IP);
    server.sin_port = htons(GAME_PORT);

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        printf("[Thread %d]Error at connect().\n", thid);
        fflush(stdout);
        return errno;
    }

    if (write(sd, currentUser, strlen(currentUser)) <= 0)
    {
        printf("[Thread %d]Error at write() to server.\n", thid);
        fflush(stdout);
        return errno;
    }

    int flag;
    if (read(sd, &flag, sizeof(int)) <= 0)
    {
        printf("[Thread %d]Error at read() from server.\n", thid);
        fflush(stdout);
        return errno;
    }

    sqlite3 *db;
    sqlite3_stmt *res;

    if (sqlite3_open(QUESDB_PATH, &db) != SQLITE_OK)
    {
        printf("[Thread %d]Error opening database: %s\n", thid, sqlite3_errmsg(db));
        fflush(stdout);
        sqlite3_close(db);
        sqlite3_finalize(res);
        return -1;
    }

    for (int q = 0; q < 5; q++)
    {
        char userAnswer[5];

        char *sql = "SELECT * FROM questions0 WHERE id = ?;";

        if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK)
        {
            printf("[Thread %d]Error executing statement: %s\n", thid, sqlite3_errmsg(db));
            fflush(stdout);
            sqlite3_close(db);
            sqlite3_finalize(res);
            return -1;
        }
        else
        {
            sqlite3_bind_int(res, 1, q + 1);
        }

        int step = sqlite3_step(res);

        const unsigned char *question = sqlite3_column_text(res, 1);
        const unsigned char *answer = sqlite3_column_text(res, 2);

        if (write(socket_fd, question, 256) <= 0)
        {
            printf("[Thread %d]Error at write() to client.\n", thid);
            fflush(stdout);
            sqlite3_close(db);
            sqlite3_finalize(res);
            return -1;
        }

        int confirmBuffer;

        bzero(userAnswer, 5);
        if (read(socket_fd, userAnswer, 5) <= 0)
        {
            printf("[Thread %d]Error at read() from client or client is not connected anymore.\n", thid);
            fflush(stdout);

            int flag = EXIT_COMMAND;
            if (write(sd, &flag, sizeof(int)) <= 0)
            {
                printf("[Thread %d]Error at write() to game.\n", thid);
                fflush(stdout);
                return -1;
            }

            if (read(sd, &confirmBuffer, sizeof(int)) <= 0)
            {
                printf("[Thread %d]Error at read() from game.\n", thid);
                fflush(stdout);
                return -1;
            }

            sqlite3_close(db);
            sqlite3_finalize(res);

            close(sd);

            return 3;
        }

        if (userAnswer[0] == 'e' && userAnswer[1] == 'x' && userAnswer[2] == 'i' && userAnswer[3] == 't')
        {
            int flag = EXIT_COMMAND;
            if (write(sd, &flag, sizeof(int)) <= 0)
            {
                printf("[Thread %d]Error at write() to game.\n", thid);
                fflush(stdout);
                return -1;
            }

            if (read(sd, &confirmBuffer, sizeof(int)) <= 0)
            {
                printf("[Thread %d]Error at read() from game.\n", thid);
                fflush(stdout);
                return -1;
            }

            sqlite3_close(db);
            sqlite3_finalize(res);

            close(sd);

            return 0;
        }
        else if (userAnswer[0] == answer[0])
        {
            int score = 10;
            if (write(sd, &score, sizeof(int)) <= 0)
            {
                printf("[Thread %d]Error at write() to game.\n", thid);
                fflush(stdout);
                sqlite3_close(db);
                sqlite3_finalize(res);
                return -1;
            }

            if (read(sd, &confirmBuffer, sizeof(int)) <= 0)
            {
                printf("[Thread %d]Error at read() from game.\n", thid);
                fflush(stdout);
                sqlite3_close(db);
                sqlite3_finalize(res);
                return -1;
            }
        }
        else if (userAnswer[0] != answer[0])
        {
            int score = 0;
            if (write(sd, &score, sizeof(int)) <= 0)
            {
                printf("[Thread %d]Error at write() to game.\n", thid);
                fflush(stdout);
                sqlite3_close(db);
                sqlite3_finalize(res);
                return -1;
            }

            if (read(sd, &confirmBuffer, sizeof(int)) <= 0)
            {
                printf("[Thread %d]Error at read() from game.\n", thid);
                fflush(stdout);
                sqlite3_close(db);
                sqlite3_finalize(res);
                return -1;
            }
        }
    }

    sqlite3_close(db);
    sqlite3_finalize(res);

    close(sd);

    sem_wait(&leaderboardSem);

    char leaderboard[512];
    bzero(leaderboard, 512);

    for (int ind = 0; ind < nrOfPlayers; ind++)
    {
        printf("[Thread %d] Building leaderboard.\n", thid);
        char line[40];
        bzero(line, 40);
        sprintf(line, "%d. %s %d\n", ind + 1, playerScores[ind].playerName, playerScores[ind].score);
        strcat(leaderboard, line);
    }

    if (write(socket_fd, leaderboard, 512) <= 0)
    {
        printf("[Thread %d]Error at write() to client.\n", thid);
        fflush(stdout);
        return -1;
    }

    return 0;
}