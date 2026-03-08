#include "socket_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int create_server(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed"); exit(1);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed"); exit(1);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // FIX: Detect Zombie processes holding the port and exit gracefully!
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("\n[FATAL ERROR] Port %d is already in use by a Zombie process!\n", port);
        printf("Please run this command in your terminal: killall -9 sat_a sat_b sat_c ground_station\n\n");
        exit(1);
    }
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed"); exit(1);
    }
    return server_fd;
}

int accept_client(int server_fd) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int client = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    return client; // Returns -1 if it fails
}

int create_client(const char *ip, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) return -1;
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { close(sock); return -1; }
    return sock;
}

void send_message(int sock, const char *message) { 
    send(sock, message, strlen(message), 0); 
}

void receive_message(int sock, char *buffer) {
    int valread = read(sock, buffer, 4095);
    if (valread > 0) buffer[valread] = '\0';
    else buffer[0] = '\0';
}

void close_socket(int sock) { close(sock); }