
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "error.h"
#include "socket_layer.h"

#define MAX_FILENAME_LENGTH 256
#define MAX_BUFFER_SIZE 2048
#define ACK "ACK"
#define BIG_FILE "Big file"
#define SMALL_FILE "Small file"



int main(int argc, char *argv[])
{
    // Check only two arguments: the program name and port
    if (argc != 2) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    // Initialise server socket
    int socket_id = tcp_server_init(atoi(argv[1]));
    if( socket_id<0) {
        perror("Error creating socket");
        close(socket_id);
        return socket_id;
    }
    printf("Server started on port %s \n", argv[1]);

    while (1) {

        // Accept connection
        int client_id = tcp_accept(socket_id);
        if (client_id <0) {
            perror("Error accepting connection \n");
            close(client_id);
            continue;
        };
        // Wait for size
        printf("Waiting for a size...\n");
        char length[6];
        if (tcp_read(client_id, length, sizeof(length))<0)  {
            perror("Error reading size from client \n");
            close(client_id);
            continue;
        }
        // Remove delimiter character
        char lengthNoDelimiter[5];
        int i;
        for(i = 0; length[i] != '|' && i < 4; i++) {
            lengthNoDelimiter[i] = length[i];
        }
        lengthNoDelimiter[i] = '\0';
        printf("Received a size: %s --> ",lengthNoDelimiter);
        // Check if size is too big
        int file_size = atoi(lengthNoDelimiter);
        if (file_size > MAX_BUFFER_SIZE) {
            if (tcp_send(client_id, BIG_FILE, strlen(BIG_FILE)) < strlen(BIG_FILE)) {
                printf("rejected\n");
                close(client_id);
                continue;
            }
        } else {
            if (tcp_send(client_id, SMALL_FILE, strlen(SMALL_FILE)) < strlen(SMALL_FILE)) {
                printf("rejected\n");
                close(client_id);
                continue;
            }
        }
        // Send acknowledgment of size

        printf("accepted\n");
        // Wait for file
        printf("About to receive a file of %d bytes\n", file_size);
        char buffer[file_size+1];
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
        char ack[3];
        strncpy(ack, "OK", 2);
        if (tcp_send(client_id, ack, 2) != 2) {
            printf("Error sending file acknowledgment to client\n");
            close(client_id);
            continue;
        }
    }
    return ERR_NONE;
}
