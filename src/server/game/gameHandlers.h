#ifndef GAMEHANDLERS_H
#define GAMEHANDLERS_H

void *gameHandler(void *arg);
int handlePlayServer(int socket_fd, int thid, char *currentUser);

#endif // GAMEHANDLERS_H