#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>
#include <sqlite3.h>
#include <sys/select.h>

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
int questionSet = 0;

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
        perror("[server]Eroare la socket().\n");
        return errno;
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
        perror("[game]Eroare la bind().\n");
        return (NULL);
    }

    if (listen(sd, 2) == -1)
    {
        perror("[game]Eroare la listen().\n");
        return (NULL);
    }

    while (1)
    {
        i_game = 0;
        nrOfPlayers = 0;

        queueData *queueData = (struct queueData *)malloc(sizeof(struct queueData));
        queueData->sd = sd;
        queueData->from = from;

        time_t seed;
        srand((unsigned)time(&seed));
        questionSet = rand() % 3;

        pthread_create(&queue_th, NULL, &queueHandler, queueData);

        printf("[game]A inceput un joc, timp de asteptare %d secunde.\n", queueTime);

        signal(SIGALRM, queueCancelHandler);
        alarm(queueTime);

        pthread_join(queue_th, NULL);

        if (nrOfPlayers == 0)
        {
            printf("[game]Nu s-au conectat destui jucatori.\n");
            fflush(stdout);
        }
        else if (nrOfPlayers > 0)
        {
            for (int i = 0; i < nrOfPlayers; i++)
            {
                void *playerScore;
                pthread_join(th_game[i], &playerScore);
                playerScores[i] = *((struct scoreData *)playerScore);
            }

            printf("[game]S-a terminat jocul.\n");

            // sortarea scorurilor
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
        }
    }
}

void queueCancelHandler(int signum)
{
    printf("[game]Timpul de asteptare a expirat.\n");
    pthread_cancel(queue_th);
}

static void *queueHandler(void *arg)
{
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
            perror("[game]Eroare la accept().\n");
            continue;
        }

        startTime = time(NULL);

        td = (struct thData *)malloc(sizeof(struct thData));
        td->idThread = i_game++;
        td->cl = client;
        td->remaningTime = endTime - startTime;
        td->endTime = endTime;

        nrOfPlayers++;
        pthread_create(&th_game[i_game], NULL, &gameTreat, td);
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
    read(tdL.cl, username, MAX_USERNAME_LENGTH);

    scoreData *playerScore;
    playerScore->score = 0;
    strcpy(playerScore->playerName, username);

    while (tdL.remaningTime < tdL.endTime) // ai putea inlocui cu sleep
    {
    }

    for (int q = 0; q < 10; q++)
    {
        char userAnswer[5];
        if (read(tdL.cl, userAnswer, 5) <= 0)
        {
            printf("[Player %d]Error at read() from client.\n", tdL.idThread);
            fflush(stdout);
            return playerScore;
        }

        if (strcmp(userAnswer, "exit") == 0)
        {
            int flag = EXIT_COMMAND;
            if (write(tdL.cl, &flag, sizeof(int)) <= 0)
            {
                printf("[Player %d]Error at write() to client.\n", tdL.idThread);
                fflush(stdout);
                return playerScore;
            }

            close((intptr_t)arg);
            return playerScore;
        }
        else if (strcmp(userAnswer, "10"))
        {
            correctAnswers++;
            int flag = GAME_CONFIRM_SUCCESS;

            if (write(tdL.cl, &flag, sizeof(int)) <= 0)
            {
                printf("[Player %d]Error at write() to client.\n", tdL.idThread);
                fflush(stdout);
                return playerScore;
            }
        }
        else if (strcmp(userAnswer, "0"))
        {
            int flag = GAME_CONFIRM_SUCCESS;

            if (write(tdL.cl, &flag, sizeof(int)) <= 0)
            {
                printf("[Player %d]Error at write() to client.\n", tdL.idThread);
                fflush(stdout);
                return playerScore;
            }
        }
    }

    close((intptr_t)arg);
    return (void *)playerScore;
}

int handlePlayServer(int socket_fd, int thid, char *currentUser)
{
    int sd;
    struct sockaddr_in server;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[player]Error at socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(HOST_IP);
    server.sin_port = htons(GAME_PORT);

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[player]Error at connect().\n");
        return errno;
    }

    if (write(sd, currentUser, strlen(currentUser)) <= 0) // poate crapa
    {
        perror("[player]Error at write() to server.\n");
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

    for (int q = 0; q < 10; q++)
    {
        char userAnswer[5];

        switch (questionSet)
        {
        case 0:
        {
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
        }
        break;

        case 1:
        {
            char *sql = "SELECT * FROM questions1 WHERE id = ?;";

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
        }
        break;

        case 2:
        {
            char *sql = "SELECT * FROM questions2 WHERE id = ?;";

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
        }
        break;

        default:
            break;
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

        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        bzero(userAnswer, 5);
        fd_set readfd;
        FD_ZERO(&readfd);
        FD_SET(socket_fd, &readfd);

        int selectResult = select(socket_fd, &readfd, NULL, NULL, &tv);
        if (selectResult == -1)
        {
            printf("[Thread %d]Error at select().\n", thid);
            fflush(stdout);
            sqlite3_close(db);
            sqlite3_finalize(res);
            return -1;
        }
        else if (selectResult == 0)
        {
            strcpy(userAnswer, "exit");
        }
        else if (selectResult > 0)
        {
            if (read(socket_fd, userAnswer, 5) <= 0)
            {
                printf("[Thread %d]Error at read() from client.\n", thid);
                fflush(stdout);
                sqlite3_close(db);
                sqlite3_finalize(res);
                return -1;
            }
        }

        int confirmBuffer;

        if (strcmp(userAnswer, "exit") == 0)
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

            break;
        }
        else if (strcmp(userAnswer, answer) == 0)
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
        else if (strcmp(userAnswer, answer) != 0)
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

    sleep(1);

    char leaderboard[1024];
    bzero(leaderboard, 1024);

    for (int i = 0; i < nrOfPlayers; i++)
    {
        char score[5], index[5];
        bzero(score, 5);
        sprintf(score, "%d", playerScores[i].score);

        bzero(index, 5);
        sprintf(index, "%d. ", i + 1);

        strcat(leaderboard, index);
        strcat(leaderboard, playerScores[i].playerName);
        strcat(leaderboard, " ");
        strcat(leaderboard, score);
        strcat(leaderboard, "\n");
    }

    return 0;
}