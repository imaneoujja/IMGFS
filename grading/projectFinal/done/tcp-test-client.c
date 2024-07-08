#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "error.h"
#include "socket_layer.h"
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_FILENAME_LENGTH 256
#define MAX_BUFFER_SIZE 2048
#define ACK "OK"
#define BIG_FILE "Big file"
#define SMALL_FILE "Small file"


int main(int argc, char *argv[])
{
    // Check for correct number of arguments: the program name , the port and the filepath
    if (argc != 3) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    // Check filename is not too long
    const char *file_path = argv[2];
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

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error creating socket");
        fclose(file);
        return EXIT_FAILURE;
    }
    // Create the server address
    int port = atoi(argv[1]);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error connecting to server");
        close(sockfd);
        fclose(file);
        return ERR_IO;
    }
    printf("Talking to %d \n", port);
    // Send length to server
    char length[6];
    sprintf(length,"%zd|",file_size);
    ssize_t bytes_sent = tcp_send(sockfd, length, sizeof(length));
    if (bytes_sent != sizeof(length)) {
        perror("Error sending size to server");
        close(sockfd);
        fclose(file);
        return ERR_IO;
    }
    printf("Sending size %ld: \n", file_size);
    // Wait for positive acknowledgement
    char ack0[strlen(SMALL_FILE)+1];
    if (tcp_read(sockfd, ack0, strlen(SMALL_FILE)) < strlen(BIG_FILE)) {
        perror("Error receiving first acknowledgment from server");
        close(sockfd);
        fclose(file);
        return ERR_IO;
    }
    printf("Server responded: %s \n", ack0);

    // Send the file
    char buffer[file_size];
    fread(buffer, 1, file_size, file);
    if (tcp_send(sockfd, buffer,file_size) != file_size) {
        close(sockfd);
        fclose(file);
        return ERR_IO;
    }
    printf("Sending %s: \n", file_path);
    // Wait for positive acknowledgement
    char ack1[3];
    ssize_t bytes_received = tcp_read(sockfd, ack1, 2);
    if (bytes_received!= 2 || strncmp(ack1, ACK,2) != 0) {
        perror("Error receiving second acknowledgment from server");
        close(sockfd);
        fclose(file);
        return ERR_IO;
    }
    printf("Accepted \n");
    printf("Done \n");
    return 0;
}
