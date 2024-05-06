#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "error.h"
#include "socket_layer.h"

#define MAX_FILENAME_LENGTH 256
#define MAX_BUFFER_SIZE 2048
#define ACK "ACK"
#define BIG_FILE "Big file"
#define SMALL_FILE "Small file"

int main(int argc, char *argv[]) {
    // Check only one argument: the port
    if (argc != 1) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    // Initialise server socket
    int socket_id = tcp_server_init(argv[0]);
    if( socket_id<0){
        perror("Error creating socket");
        close(socket_id);
        return socket_id;
    }
    printf("Server started on port %d \n", socket_id);

    while (1) {

        // Accept connection
        int client_id = tcp_accept(socket_id);
        if (client_id <0){
            perror("Error accepting connection \n");
            close(client_id);
            continue;
        };
        // Wait for size
        printf("Waiting for a size...\n");
        long file_size;
        if (tcp_read(client_id, (char*) &file_size), sizeof(file_size)<0)  {
            perror("Error reading size from client \n");
            close(client_id);
            continue;
        }
        printf("Received a size: %s --> ",file_size);
        // Check if size is too big
        char ack[sizeof(SMALL_FILE)];
        if (atoi(file_size) > MAX_BUFFER_SIZE) {
            strncpy(ack, BIG_FILE, sizeof(BIG_FILE));
        } else {
            strncpy(ack, SMALL_FILE, sizeof(SMALL_FILE));
        }
        // Send acknowledgment of size
        if (tcp_send(client_id, ack, sizeof(ack)) < sizeof(BIG_FILE)) {
            printf("rejected\n");
            close(client_id);
            continue;
        }
        printf("accepted\n");
        // Wait for file
        printf("About to receive a file of %d bytes\n", file_size);
        char buffer[file_size];
        ssize_t bytes_received = tcp_read(client_id, buffer, file_size);
        // Check if file was received correctly
        if (bytes_received != file_size) {
            printf("Error reading file from client\n");
            close(client_id);
            continue;
        }
        buffer[bytes_received] = '\0';
        printf("Received a file:\n%s\n", buffer);

        // Send acknowledgment
        strncpy(ack, "OK", sizeof("OK"));
        if (tcp_send(client_id, ack, sizeof("OK")) != sizeof("OK")) {
            printf("Error sending file acknowledgment to client\n");
            close(client_id);
            continue;
        }
    }
    return ERR_NONE;
}
