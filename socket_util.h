#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

int create_server(int port);
int accept_client(int server_fd);
int create_client(const char *ip, int port);
void send_message(int sock, const char *message);
void receive_message(int sock, char *buffer);
void close_socket(int sock);

#endif