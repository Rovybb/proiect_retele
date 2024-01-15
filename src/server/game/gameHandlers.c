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
int questionSet = 0;

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
        perror("[server]Eroare la socket().\n");
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
        questionSet = 0; /////////////////////////////////////////////////

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
            sem_init(&leaderboardSem, 0, 1);

            for (int i = 0; i < nrOfPlayers; i++)
            {
                void *playerScore;
                playerScore = (struct scoreData *)malloc(sizeof(struct scoreData));
                if (pthread_join(th_game[i], &playerScore) != 0)
                {
                    printf("[game]Eroare la pthread_join().\n");
                    fflush(stdout);
                }
                if (strcmp(((struct scoreData *)playerScore)->playerName, "exit") != 0)
                {
                    playerScores[i] = *((struct scoreData *)playerScore);
                }
                free(playerScore);
            }

            printf("[game]S-a terminat jocul.\n");

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

            sem_post(&leaderboardSem);
            sem_destroy(&leaderboardSem);
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
        td->idThread = i_game;
        td->cl = client;
        td->remaningTime = endTime - startTime;
        td->endTime = endTime;

        nrOfPlayers++;
        pthread_create(&th_game[i_game], NULL, &gameTreat, td);
        i_game++;
        printf("[game]S-a conectat un nou jucator.\n");
        printf("Th id %d : %lu\n", i_game - 1, th_game[i_game - 1]);
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

    printf("[Player %d]Username: %s\n", tdL.idThread, username);

    scoreData *playerScore;
    playerScore = (struct scoreData *)malloc(sizeof(struct scoreData));
    playerScore->score = 0;
    strcpy(playerScore->playerName, username);

    printf("[Player %d]Asteptam inceperea jocului. timp: %d\n", tdL.idThread, (int)tdL.remaningTime);

    sleep((unsigned int)tdL.remaningTime);

    printf("[Player %d]Jocul a inceput.\n", tdL.idThread);
    fflush(stdout);

    int flag = GAME_START;
    if (write(tdL.cl, &flag, sizeof(int)) <= 0)
    {
        printf("[Player %d]Error at write() to client.\n", tdL.idThread);
        fflush(stdout);
        return NULL;
    }

    for (int q = 0; q < 5; q++) ///////////////////////////////////////
    {
        int userAnswer;
        if (read(tdL.cl, &userAnswer, sizeof(int)) <= 0)
        {
            printf("[Player %d]Error at read() from client.\n", tdL.idThread);
            fflush(stdout);
            return playerScore;
        }

        printf("[Player %d]Answer: %d\n", tdL.idThread, userAnswer);

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

    printf("[player]Connected to game.\n");

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

    for (int q = 0; q < 5; q++) // 100000000000000000000
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

        bzero(userAnswer, 5);
        if (read(socket_fd, userAnswer, 5) <= 0)
        {
            printf("[Thread %d]Error at read(co) from client.\n", thid);
            fflush(stdout);
            sqlite3_close(db);
            sqlite3_finalize(res);
            return -1;
        }

        int confirmBuffer;

        printf("[Thread %d]Answer: %s Correct answer: %s\n", thid, userAnswer, answer);

        if (userAnswer[0] == 'e')
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

    char leaderboard[1024];
    bzero(leaderboard, 1024);

    for (int i = 0; i < nrOfPlayers; i++)
    {
        char line[100];
        bzero(line, 100);
        sprintf(line, "%d. %s %d\n", i + 1, playerScores[i].playerName, playerScores[i].score);
        strcat(leaderboard, line);
    }

    if (write(socket_fd, leaderboard, 1024) <= 0)
    {
        printf("[Thread %d]Error at write() to client.\n", thid);
        fflush(stdout);
        return -1;
    }

    return 0;
}