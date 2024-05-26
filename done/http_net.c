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

/***********************
 * Handle connection
 */
static void* handle_connection(void *arg) {
    if (arg == NULL ){
        return &our_ERR_INVALID_ARGUMENT;
    }
    int c_socket = (intptr_t)arg;

    char *rcvbuf = calloc(1, MAX_HEADER_SIZE);
    if (!rcvbuf) {
        return &our_ERR_OUT_OF_MEMORY;
    }

    ssize_t bytes_read = 0;
    struct http_message msg;
    memset(&msg, 0, sizeof(msg));
    int end_header = 0;
    int content_length = 0;
    int parse_result = 0;

    while (1) {
        ssize_t result = tcp_read(c_socket, rcvbuf + bytes_read, MAX_HEADER_SIZE - bytes_read);
        if (result < 0) {
            perror("Failed to read from socket");
            close(c_socket);
            free(rcvbuf);
            rcvbuf = NULL;
            return &our_ERR_IO;
        }
        bytes_read += result;

        // Check if we have received the entire header
        if (!end_header && strstr(rcvbuf, HTTP_HDR_END_DELIM) != NULL) {
            end_header = 1;
        }

        parse_result = http_parse_message(rcvbuf, bytes_read, &msg, &content_length);
        if (parse_result < 0) {
            close(c_socket);
            free(rcvbuf);
            rcvbuf = NULL;
            return &parse_result;
        } else if (parse_result == 0) {
            // If we have a partial message, we need to read more data
            if (content_length > 0 && bytes_read < MAX_HEADER_SIZE + content_length) {
                // Extend the buffer if needed and continue reading
                rcvbuf = realloc(rcvbuf, MAX_HEADER_SIZE + content_length);
                if (!rcvbuf) {
                    perror("Failed to reallocate memory for receive buffer");
                    close(c_socket);
                    return &our_ERR_OUT_OF_MEMORY;
                }
                continue;
            }
        }

        if (parse_result > 0 || end_header) {
            // Full message received, call the callback
            int callback_result = cb(&msg, c_socket);
            if (callback_result != ERR_NONE) {
                close(c_socket);
                free(rcvbuf);
                return &callback_result;
            }
            break;
        }
    }

    close(c_socket);
    free(rcvbuf);
    return &our_ERR_NONE;

}


/***********************
 * Init connection
 */
int http_init(uint16_t port, EventCallback callback)
{
    passive_socket = tcp_server_init(port);
    cb = callback;
    return passive_socket;
}

/***********************
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

/***********************
 * Receive content
 */
int http_receive(void) {
    
    int client_socket = tcp_accept(passive_socket);
    if (client_socket < 0) {
        close(passive_socket);
        return ERR_IO;
    }
    handle_connection((void*)&client_socket);
    
    return ERR_NONE;
}

/***********************
 * Serve a file content over HTTP
 */
int http_serve_file(int connection, const char* filename)
{
    int ret = ERR_NONE;
    return ret;
}

/***********************
 * Create and send HTTP reply
 */
int http_reply(int connection, const char* status, const char* headers, const char* body, size_t body_len) {
    M_REQUIRE_NON_NULL(status);
    M_REQUIRE_NON_NULL(headers);
    M_REQUIRE_NON_NULL(body);


    char* buffer = calloc (1,MAX_REQUEST_SIZE);
    sprintf(buffer, "%s %s\r\n%sContent-Length: %zu\r\n\r\n",
                                  HTTP_PROTOCOL_ID, status, HTTP_LINE_DELIM , 
                                  headers, body_len , HTTP_HDR_END_DELIM, body);

    ssize_t sent = tcp_send(connection, buffer, MAX_REQUEST_SIZE);
     
     if(sent < 0){
        free(buffer);
        buffer = NULL;
        close(connection);
        return ERR_IO;
    }
    return ERR_NONE;


}