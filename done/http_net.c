/*
 * @file http_net.c
 * @brief HTTP server layer for CS-202 project
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "error.h"

static int passive_socket = -1;
static EventCallback cb;

#define MK_OUR_ERR(X) \
static int our_ ## X = X

MK_OUR_ERR(ERR_NONE);
MK_OUR_ERR(ERR_INVALID_ARGUMENT);
MK_OUR_ERR(ERR_OUT_OF_MEMORY);
MK_OUR_ERR(ERR_IO);

/*******************************************************************
 * Handle connection
 */
static void* handle_connection(void *arg) {
    M_REQUIRE_NON_NULL(arg);
    int client_socket = *(int*)arg;
    char buffer[MAX_HEADER_SIZE] = {0};
    int read_bytes = tcp_read(client_socket, buffer, MAX_HEADER_SIZE - 1);
    if (read_bytes < 0) {
        close(client_socket);
        return &our_ERR_IO;
    }

    buffer[read_bytes] = '\0';  // Ensure null termination
    if (strstr(buffer, "test: ok")) {
        http_reply(client_socket, HTTP_OK, "Content-Type: text/plain" HTTP_LINE_DELIM, "Hello, world!", 12);
    } else {
        http_reply(client_socket, HTTP_BAD_REQUEST, NULL, "Bad Request", 11);
    }

    close(client_socket);
    return ERR_NONE;
}


/*******************************************************************
 * Init connection
 */
int http_init(uint16_t port, EventCallback callback)
{
    passive_socket = tcp_server_init(port);
    cb = callback;
    return passive_socket;
}

/*******************************************************************
 * Close connection
 */
void http_close(void)
{
    if (passive_socket > 0) {
        if (close(passive_socket) == -1)
            perror("close() in http_close()");
        else
            passive_socket = -1;
    }
}

/*******************************************************************
 * Receive content
 */
int http_receive(void) {
    while (1) {
        int client_socket = tcp_accept(passive_socket);
        if (client_socket < 0) {
            perror("Error accepting connection");
            continue;
        }
        handle_connection((void*)&client_socket);
    }
    return ERR_NONE;
}

/*******************************************************************
 * Serve a file content over HTTP
 */
int http_serve_file(int connection, const char* filename)
{
    int ret = ERR_NONE;
    return ret;
}

/*******************************************************************
 * Create and send HTTP reply
 */
int http_reply(int connection, const char* status, const char* headers, const char* body, size_t body_len) {
    M_REQUIRE_NON_NULL(status);
    M_REQUIRE_NON_NULL(headers);
    M_REQUIRE_NON_NULL(body);
    size_t header_size = snprintf(NULL, 0, "%s %s\r\n%sContent-Length: %zu\r\n\r\n",
                                  HTTP_PROTOCOL_ID, status, headers ? headers : "", body_len) + 1;

    char* response = malloc(header_size + body_len);
    if (!response) {
        return ERR_OUT_OF_MEMORY;
    }

    snprintf(response, header_size, "%s %s\r\n%sContent-Length: %zu\r\n\r\n",
             HTTP_PROTOCOL_ID, status, headers ? headers : "", body_len);

    // Append the body part
    if (body && body_len > 0) {
        memcpy(response + header_size - 1, body, body_len);
    }


    int send_result = tcp_send(connection, response, header_size + body_len - 1);
    free(response);
    return send_result;
}
