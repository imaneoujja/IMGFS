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
#include "imgfs_server_service.h"
#include "error.h"

static int passive_socket = -1;
static EventCallback cb = handle_http_message;

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
    if (arg == NULL){
        return &our_ERR_INVALID_ARGUMENT;
    }
    char* rcvbuf = calloc(1, MAX_HEADER_SIZE +1);
    if (rcvbuf == NULL) return &our_ERR_OUT_OF_MEMORY;
    char* nxt_message_buf = calloc(1, MAX_HEADER_SIZE +1);
    if (nxt_message_buf == NULL) return &our_ERR_OUT_OF_MEMORY;
    int end_header = 0;
    int* c_socket = (intptr_t)arg;
    ssize_t bytes_read = 0;
    struct http_message msg;
    memset(&msg, 0, sizeof(msg));
    int content_length = 0;
    int extended = 0;
    int previous_bytes_read = 0;
    int parse_result;
    size_t bytes_message_read = 0;
    char* endpos;

    while(!end_header && bytes_message_read < MAX_HEADER_SIZE){
        if (previous_bytes_read==0){
            bytes_read = tcp_read(*c_socket, rcvbuf + bytes_message_read, MAX_HEADER_SIZE );
            if (bytes_read < 0) {
                perror("Failed to read from socket");
                close(*c_socket);
                free(rcvbuf);
                rcvbuf= NULL;
                return &our_ERR_IO;
            }
            if ((endpos = strstr(rcvbuf + bytes_message_read, HTTP_HDR_END_DELIM))!=NULL && endpos<rcvbuf + bytes_message_read+bytes_read) {
                end_header = 1;
                bytes_read = endpos - rcvbuf - bytes_message_read + strlen(HTTP_HDR_END_DELIM);
                strncpy(nxt_message_buf, endpos + strlen(HTTP_HDR_END_DELIM), MAX_HEADER_SIZE-bytes_read);
                previous_bytes_read = MAX_HEADER_SIZE-bytes_read;
            }
        }
        else{
            strncpy(rcvbuf, nxt_message_buf, MAX_HEADER_SIZE);
            previous_bytes_read = 0;
        }


        parse_result = http_parse_message(rcvbuf + bytes_message_read, bytes_read, &msg, &content_length);
        if (parse_result < 0) {
            perror("Failed to parse message");
            close(*c_socket);
            free(rcvbuf);
            rcvbuf=NULL;
            return (void*)(intptr_t)parse_result;
        }
        if (parse_result == 0 && !extended && content_length > 0 && bytes_read <content_length) {
                rcvbuf = realloc(rcvbuf, MAX_HEADER_SIZE + content_length);
                bytes_message_read += bytes_read;
                if (rcvbuf == NULL) {
                    perror("Failed to realloc buffer");
                    close(*c_socket);
                    free(rcvbuf);
                    return &our_ERR_OUT_OF_MEMORY;
                }
                extended = 1;

        }
        else if (parse_result > 0) {
            if (cb) {
                cb(&msg, *c_socket);
            }
            memset(rcvbuf, 0, MAX_HEADER_SIZE  );
            memset(&msg, 0, sizeof(msg));
            bytes_read = 0;
            bytes_message_read = 0;
            content_length = 0;
            end_header = 0;
        }
    }


    close(*c_socket);
    free(rcvbuf);
    rcvbuf = NULL;
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
    if (body_len !=0) M_REQUIRE_NON_NULL(body);

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