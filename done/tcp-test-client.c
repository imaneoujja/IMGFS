#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "error.h"
#include <errno.h>
#include "socket_layer.h"

#define MAX_FILENAME_LENGTH 256
#define MAX_BUFFER_SIZE 2048
#define ACK "ACK"
#define BIG_FILE "Big file";
#define SMALL_FILE "Small file";


int main(int argc, char *argv[]) {
    // Check for correct number of arguments: the port and the filepath
    if (argc != 2) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    // Check filename is not too long
    char *file_path = argv[1];
    if (strnlen(file_path, MAX_FILENAME_LENGTH) == MAX_FILENAME_LENGTH) {
        return ERR_INVALID_ARGUMENT;
    }
    // Open the file
    FILE *file= fopen(file_path, "rb");
    if (file == NULL) {
        return ERR_IO;
    }

    // Get  the file size and check that it is below 2048
    fseek(file, 0, SEEK_END);
    ssize_t file_size = (ssize_t) ftell(file);
    fseek(file, 0, SEEK_SET);
    if (file_size >= MAX_BUFFER_SIZE) {
        return ERR_INVALID_ARGUMENT;
    }
    int socket_id = atoi(argv[0]);
    printf("Talking to %d \n", socket_id);
    // Send length to server
    if (tcp_send(socket_id, (const char *)&file_size, sizeof(file_size)) != sizeof(file_size)) {
        perror("Error sending size to server");
        close(socket_id);
        fclose(file);
        return ERR_IO;
    }
    printf("Sending size %ld: \n", file_size);
    // Wait for positive acknowledgement
    char ack0[sizeof(SMALL_FILE)];
    if (tcp_read(socket_id, ack0, sizeof(ack0)) != sizeof(ack0) || strcmp(ack0, ACK) != 0){
        perror("Error receiving first acknowledgment from server");
        close(socket_id);
        fclose(file);
        return ERR_IO;
    }
    printf("Server responded: %s \n", ack0);

    // Send the file
    if (tcp_send(socket_id, (const char *)&file_size,file_size) != file_size) {
        close(socket_id);
        fclose(file);
        return ERR_IO;
    }
    printf("Sending %s: \n", file_path);
    // Wait for positive acknowledgement
    char ack1[sizeof(ACK)];
    if (tcp_read(socket_id, ack1, sizeof(ack1)) != sizeof(ack1) || strcmp(ack1, ACK) != 0){
        perror("Error receiving second acknowledgment from server");
        close(socket_id);
        fclose(file);
        return ERR_IO;
    }
    printf("Accepted \n");

    return 0;
}