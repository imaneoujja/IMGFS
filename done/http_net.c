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
#include <pthread.h>
#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "imgfs_server_service.h"
#include "error.h"

static int passive_socket = -1;
static EventCallback cb;

#define MK_OUR_ERR(X) \
static int our_ ## X = X

MK_OUR_ERR(ERR_NONE);
MK_OUR_ERR(ERR_INVALID_ARGUMENT);
MK_OUR_ERR(ERR_OUT_OF_MEMORY);
MK_OUR_ERR(ERR_IO);

static void* handle_connection(void *arg) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT );
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if (arg == NULL) return &our_ERR_INVALID_ARGUMENT;
    int socketId = *(int *)arg;
    char *rcvbuf = calloc(1, MAX_HEADER_SIZE);
    if (!rcvbuf) {
        return &our_ERR_OUT_OF_MEMORY;
    }

    ssize_t bytes_read;
    size_t total = 0;
    int content_len = 0;
    struct http_message http_msg = {0};
    int parsed_message;
    int buffer = MAX_HEADER_SIZE;
    int extended = 0;

    while (1) {
        bytes_read = tcp_read(socketId, rcvbuf + total, buffer);
        if (bytes_read < 0) {
            free(rcvbuf);
            return &our_ERR_IO;
        } else if (bytes_read == 0) break;

        total += bytes_read;

        parsed_message = http_parse_message(rcvbuf, total, &http_msg, &content_len);
        if (parsed_message < 0) {
            free(rcvbuf);
            return &our_ERR_INVALID_ARGUMENT;
        }

        if (parsed_message == 0 && !extended && content_len > 0 && total < content_len + MAX_HEADER_SIZE) {
            buffer = content_len + MAX_HEADER_SIZE;
            char *new_buf = realloc(rcvbuf, buffer);
            if (new_buf == NULL) {
                free(rcvbuf);
                return &our_ERR_OUT_OF_MEMORY;
            }
            rcvbuf = new_buf;
            extended = 1;
        }

        if (parsed_message == 1) {
            cb(&http_msg, socketId);

            memset(rcvbuf, 0, buffer);
            total = 0;
            content_len = 0;
            parsed_message = 0;
            extended = 0;
        }

        if (total >= buffer) {
            free(rcvbuf);
            return &our_ERR_IO;
        }
    }

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

/*******************************************************************
 * Thread function to handle the connection
 */
void* thread_func(void* arg) {
    int* client_socket = (int*)arg;
    printf("client_socket: %d\n", *client_socket);
    handle_connection(client_socket);
    return NULL;
}

/*******************************************************************
 * Receive content
 */
int http_receive(void) {
    // All created threads will have same attributes
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int* active_socket = calloc(1,sizeof(int));
    if (active_socket == NULL) {
        perror("Error allocating memory for active socket");
        pthread_attr_destroy(&attr);
        return ERR_OUT_OF_MEMORY;
    }
    while (1) {
        // Accept connection
        *active_socket = tcp_accept(passive_socket);
        printf("active_socket: %d\n", *active_socket);
        if (*active_socket < 0) {
            perror("Error accepting connection");
            close(*active_socket);
            continue;
        }
        // Create thread
        pthread_t thread;
        if (pthread_create(&thread, &attr, thread_func, active_socket) != 0) {
            perror("Error creating thread");
            close(*active_socket);
        }

    }

    pthread_attr_destroy(&attr);
    return ERR_NONE;
}


/***********************
 * Serve a file content over HTTP
 */
int http_serve_file(int connection, const char* filename)
{
    M_REQUIRE_NON_NULL(filename);

    // open file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to open file \"%s\"\n", filename);
        return http_reply(connection, "404 Not Found", "", "", 0);
    }

    // get its size
    fseek(file, 0, SEEK_END);
    const long pos = ftell(file);
    if (pos < 0) {
        fprintf(stderr, "http_serve_file(): Failed to tell file size of \"%s\"\n",
                filename);
        fclose(file);
        return ERR_IO;
    }
    rewind(file);
    const size_t file_size = (size_t) pos;

    // read file content
    char* const buffer = calloc(file_size + 1, 1);
    if (buffer == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to allocate memory to serve \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    const size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "http_serve_file(): Failed to read \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    // send the file
    const int  ret = http_reply(connection, HTTP_OK,
                                "Content-Type: text/html; charset=utf-8" HTTP_LINE_DELIM,
                                buffer, file_size);

    // garbage collecting
    fclose(file);
    free(buffer);
    return ret;
}

/***********************
 * Create and send HTTP reply
 */
int http_reply(int connection, const char* status, const char* headers, const char* body, size_t body_len) {
    M_REQUIRE_NON_NULL(status);
    M_REQUIRE_NON_NULL(headers);
    
    if (body_len !=0) M_REQUIRE_NON_NULL(body);

    size_t rcvbuf = strlen(HTTP_PROTOCOL_ID) + strlen(status)
                        + strlen(HTTP_LINE_DELIM) + strlen(headers)
                        + strlen("Content-Length: ")
                        + ((body_len == 0) ? 1 : (int)(log10(body_len) + 1))
                        + strlen(HTTP_HDR_END_DELIM) + body_len + 1;

    char* buffer = (char*)calloc(rcvbuf, 1);
    if (buffer == NULL) return our_ERR_OUT_OF_MEMORY;

    int content = snprintf(buffer, rcvbuf, "%s%s%s%s%s%zu%s", HTTP_PROTOCOL_ID, status, 
                  HTTP_LINE_DELIM, headers, "Content-Length: ", body_len, HTTP_HDR_END_DELIM);

    if (content < 0 || content >= rcvbuf) {
        free(buffer);
        return our_ERR_INVALID_ARGUMENT;
    }

    if (body != NULL)  memcpy(buffer + content, body, body_len);
    
    int total = 0;

    while (total < content + body_len) {
        ssize_t sent = tcp_send(connection, buffer + total, content + body_len - total);
        if (sent < 0) {
            free(buffer);
            return ERR_IO;
        }
        total += sent;
    }
    free(buffer);
    return our_ERR_NONE;
}