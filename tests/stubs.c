#include "../socket_util.h"
#include <stdio.h>
#include <stdlib.h>

int create_server(int port) { return -1; }
int accept_client(int server_fd) { return -1; }
int create_client(const char *ip, int port) { return -1; }
void send_message(int sock, const char *message) {}
void receive_message(int sock, char *buffer) {}
void close_socket(int sock) {}
