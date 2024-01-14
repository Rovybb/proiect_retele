#ifndef SERVERHANDLERS_H
#define SERVERHANDLERS_H

int handleLoginServer(int socket, int thid, char *currentUser);
int handleRegisterServer(int socket, int thid);
int handleLogoutServer(int socket, int thid);

#endif // SERVERHANDLERS_H