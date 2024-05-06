#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "error.h"

/**
 * @brief initialize a network communication over TCP
 *
 * @param uint16_t Structure for header, metadata and file pointer to be freed/closed.
 * @return socket id
 */
int tcp_server_init(uint16_t port){
    // Create a socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if  (sock_fd == -1){
        perror("Error creating socket");
        return ERR_IO;
    }
    // Create the server address
    struct sockaddr_in* addr;
    memset(addr, 0, sizeof(addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = INADDR_LOOPBACK;
    // Bind the socket to the address
    if (bind(sock_fd, addr,sizeof(struct sockaddr_in)) != ERR_NONE){
        perror("Error binding socket");
        close(sock_fd);
        return ERR_IO;
    }
    // Listen for incoming connections
    if (listen(sock_fd, 10)!=ERR_NONE){
        perror("Error listening on socket");
        close(sock_fd);
        return ERR_IO;
    }
    return sock_fd;
}

/**
 * @brief Blocking call that accepts a new TCP connection
 * @param passive_socket the file descriptor of the socket that listens for new connections
 * @return the socket id of the new connection
 */
int tcp_accept(int passive_socket){
    return accept(passive_socket, NULL, NULL);
}

/**
 * @brief Blocking call that reads the active socket once and stores the output in buf
 * @param active_socket the file descriptor of the socket that is being read
 * @param buf the buffer where the data will be stored
 * @param buflen the size of the buffer
 * @return the number of bytes read or -1 on error
 */
ssize_t tcp_read(int active_socket, char* buf, size_t buflen){
    // Check validity of arguments
    M_REQUIRE_NON_NULL(buf);
    if (buflen == 0 || active_socket < 0) {
        return ERR_INVALID_ARGUMENT;
    }
    return recv(active_socket, buf, buflen, 0);
}

/**
 * @brief Send a response message
 * @param active_socket the file descriptor of the socket that is sending the message
 * @param response the message to be sent
 * @param response_len the size of the message to be sent
 * @return the number of bytes sent or -1 on error
 */
ssize_t tcp_send(int active_socket, const char* response, size_t response_len){
    // Check validity of arguments
    M_REQUIRE_NON_NULL(response);
    if (response_len == 0 || active_socket < 0) {
        return ERR_INVALID_ARGUMENT;
    }
    return send(active_socket, response, response_len, 0);
}
