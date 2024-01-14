#ifndef CLIENTHANDLERS_H
#define CLIENTHANDLERS_H

int handleLogin(int socket);
int handleRegister(int socket);
int handleLogout(int socket);
int handlePlay(int socket);

#endif // CLIENTHANDLERS_H