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



static void* handle_connection(void *arg) {
    if (arg == NULL) return &our_ERR_INVALID_ARGUMENT;

    char* rcvbuf = calloc(1, MAX_HEADER_SIZE );
    if (rcvbuf == NULL) return &our_ERR_OUT_OF_MEMORY;
    int end_header = 0;
    int* c_socket = (intptr_t)arg;
    ssize_t bytes_read = 0;
    struct http_message msg;
    memset(&msg, 0, sizeof(msg));
    int content_length = 0;
    int extended = 0;
    ssize_t total =0;
    int parse_result;
    char* endpos;
    int header_end = 0;


    while(!end_header && total < MAX_HEADER_SIZE){
        bytes_read = tcp_read(*c_socket, rcvbuf + total, MAX_HEADER_SIZE  -total - 1);
        if (bytes_read < 0) {
            close(*c_socket);
            free(rcvbuf);
            rcvbuf= NULL;
        }
        total += bytes_read;

        if ((endpos = strstr(rcvbuf , HTTP_HDR_END_DELIM))!=NULL && endpos<rcvbuf + total + bytes_read){
            end_header = 1;
            header_end = endpos - rcvbuf - total + strlen(HTTP_HDR_END_DELIM);
        }

        parse_result = http_parse_message(rcvbuf , total, &msg, &content_length);
        if (parse_result < 0) {
            close(*c_socket);
            free(rcvbuf);
            rcvbuf=NULL;

        }
        if (parse_result == 0 && !extended && content_length > 0 && total - header_end <content_length) {
            char* new_buff = realloc(rcvbuf, MAX_HEADER_SIZE + content_length);
            total+= bytes_read;
            if (rcvbuf == NULL) {
                perror("Failed to realloc buffer");
                close(*c_socket);
                free(rcvbuf);
                rcvbuf=NULL;
            }
            rcvbuf = new_buff;
            extended = 1;

        }
        else if (parse_result > 0) {
            if (cb) {
                cb(&msg, *c_socket);
                memset(&msg, 0, sizeof(msg));
                total = 0;
                content_length = 0;
                extended = 0;
                header_end = 0;

            }
            else{
                close(*c_socket);
                free(rcvbuf);
                rcvbuf=NULL;

            }


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