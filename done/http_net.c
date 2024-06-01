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
    // Avoid SIGINT and SIGTERM signals (left to the main thread)
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT );
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if (arg == NULL) return &our_ERR_INVALID_ARGUMENT;
    int socketId = *(int *)arg;
// agv free, ;clsose(socketID)
    char *rcvbuf = calloc(1, MAX_HEADER_SIZE);
    if (!rcvbuf) {
        perror("Failed to allocate buffer");
        return &our_ERR_OUT_OF_MEMORY;
    }

    ssize_t bytes_read;
    size_t total_read = 0;
    int content_length = 0;
    struct http_message http_msg = {0};
    int parse_result;
    int buffer_size = MAX_HEADER_SIZE;
    int extension_done = 0;

    while (1) {
        bytes_read = tcp_read(socketId, rcvbuf + total_read, buffer_size);
        if (bytes_read < 0) {
            perror("tcp_read error");
            free(rcvbuf);
            return &our_ERR_IO;
        } else if (bytes_read == 0) break;

        total_read += bytes_read;

        parse_result = http_parse_message(rcvbuf, total_read, &http_msg, &content_length);
        if (parse_result < 0) {
            free(rcvbuf);
            return &our_ERR_INVALID_ARGUMENT;
        }

        if (parse_result == 0 && !extension_done && content_length > 0 && total_read < content_length + MAX_HEADER_SIZE) {
            buffer_size = content_length + MAX_HEADER_SIZE;
            char *new_buf = realloc(rcvbuf, buffer_size);
            if (new_buf == NULL) {
                perror("Failed to extend buffer");
                free(rcvbuf);
                return &our_ERR_OUT_OF_MEMORY;
            }
            rcvbuf = new_buf;
            extension_done = 1;
        }

        if (parse_result == 1) {
            cb(&http_msg, socketId);

            memset(rcvbuf, 0, buffer_size);
            total_read = 0;
            content_length = 0;
            parse_result = 0;
            extension_done = 0;
        }

        if (total_read >= buffer_size) {
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
    printf("HELLO\n");

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

    const char* protocol = HTTP_PROTOCOL_ID;
    const char* content_length_text = "Content-Length: ";
    int body_len_chars = (body_len == 0) ? 1 : (int)(log10(body_len) + 1);

    size_t buffer_size = strlen(HTTP_PROTOCOL_ID)
                        + strlen(status)
                        + strlen(HTTP_LINE_DELIM)
                        + strlen(headers)
                        + strlen(content_length_text)
                        + body_len_chars
                        + strlen(HTTP_HDR_END_DELIM)
                        + body_len
                        + 1;

    char* buffer = (char*)calloc(buffer_size, 1);
    if (buffer == NULL) {
        perror("Failed to allocate memory for response buffer");
        return our_ERR_OUT_OF_MEMORY;
    }

    int written = snprintf(buffer, buffer_size, "%s%s%s%s%s%zu%s", protocol,
                           status, HTTP_LINE_DELIM, headers,
                           content_length_text, body_len, HTTP_HDR_END_DELIM);

    if (written < 0 || written >= buffer_size) {
        perror("Failed to write response header");
        free(buffer);
        return our_ERR_INVALID_ARGUMENT;
    }

    if (body != NULL) {
        memcpy(buffer + written, body, body_len);
    }
    int total_sent = 0;
    // Send the response in a loop to handle partial sends
    while (total_sent < written + body_len) {
        ssize_t sent = tcp_send(connection, buffer + total_sent, written + body_len - total_sent);
        if (sent < 0) {
            perror("Failed to send complete response");
            free(buffer);
            return ERR_IO;
        }
        total_sent += sent;
    }
    free(buffer);
    perror("HELLO");
    return our_ERR_NONE;

 

}