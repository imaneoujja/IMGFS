/*
 * @file http_net.c
 * @brief HTTP server layer for CS-202 project
 *
 * @author Marta Adarve de Leon & Imane Oujja
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

/***********************
 * Handle multiple HTTP messages
 */
static void* handle_connection(void *arg)
{
    // Avoid SIGINT and SIGTERM signals (left to the main thread)
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT );
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if (arg == NULL) return &our_ERR_INVALID_ARGUMENT;

    // Initialise all variables
    int client_socket = *(int *)arg;
    char *rcvbuf = malloc(MAX_HEADER_SIZE + 1);
    if (rcvbuf == NULL) return &our_ERR_OUT_OF_MEMORY;
    size_t total = 0;
    int content_len = 0;
    char *header_end = NULL;
    int extended = 0;

    struct http_message message;

    while (1) {
        // Read from the socket
        ssize_t bytes_read = tcp_read(client_socket,
                                      rcvbuf + total,
                                      MAX_HEADER_SIZE + content_len - total);
        if (bytes_read < 0) {
            free(rcvbuf);
            rcvbuf = NULL;
            close(client_socket);

            return &our_ERR_IO;
        }
        if(bytes_read==0) break;
        // Check if the header is complete
        if (header_end == NULL) {
            header_end = strstr(rcvbuf, HTTP_HDR_END_DELIM);
            if (header_end != NULL) {
                header_end += strlen(HTTP_HDR_END_DELIM);
            }
        }

        // Update total bytes read for this http message
        total += bytes_read;
        // Check if now message is complete
        int ret_parsed_mess = http_parse_message(rcvbuf,total,&message, &content_len);
        if (ret_parsed_mess < 0) {
            free(rcvbuf);
            rcvbuf = NULL;
            close(client_socket);
            return ret_parsed_mess;
        } else if (ret_parsed_mess == 0) { // Parsed message is not complete
            // Check if we need to extend the buffer
            if (!extended && content_len > 0 && rcvbuf+total -header_end< content_len) {
                char *new_buf = realloc(rcvbuf, MAX_HEADER_SIZE + content_len);
                if (!new_buf) {
                    free(rcvbuf);
                    rcvbuf = NULL;
                    close(client_socket);

                    return &our_ERR_OUT_OF_MEMORY;
                }
                rcvbuf = new_buf;
                extended = 1;
            }
        } else {
            // Call the callback function
            int callback_result = cb(&message, client_socket);
            if (callback_result < 0) {
                free(rcvbuf);
                rcvbuf = NULL;
                close(client_socket);
                return &our_ERR_IO;

            } else {
                total = 0;
                content_len = 0;
                extended = 0;
                header_end = NULL;
                memset(rcvbuf, 0, MAX_HEADER_SIZE);
            }
        }
    }

    free(rcvbuf);
    rcvbuf = NULL;
    close(client_socket);

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
void* thread_func(void* arg)
{
    int* client_socket = (int*)arg;
    printf("client_socket: %d\n", *client_socket);
    handle_connection(client_socket);
    return NULL;
}

/*******************************************************************
 * Receive content
 */
int http_receive(void)
{
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
int http_reply(int connection, const char* status, const char* headers, const char* body, size_t body_len)
{
    M_REQUIRE_NON_NULL(status);
    M_REQUIRE_NON_NULL(headers);
    if (body_len !=0) M_REQUIRE_NON_NULL(body);

    // Calculate the size of the buffer needed
    size_t rcvbuf = strlen(HTTP_PROTOCOL_ID) + strlen(status)
                    + strlen(HTTP_LINE_DELIM) + strlen(headers)
                    + strlen("Content-Length: ")
                    + ((body_len == 0) ? 1 : (int)(log10(body_len) + 1))
                    + strlen(HTTP_HDR_END_DELIM) + body_len + 1;

    char* buffer = (char*)calloc(rcvbuf, 1);
    if (buffer == NULL) return our_ERR_OUT_OF_MEMORY;

    // Create the HTTP response header
    int content = snprintf(buffer, rcvbuf, "%s%s%s%s%s%zu%s", HTTP_PROTOCOL_ID, status,
                           HTTP_LINE_DELIM, headers, "Content-Length: ", body_len, HTTP_HDR_END_DELIM);

    if (content < 0 || content >= rcvbuf) {
        free(buffer);
        return our_ERR_INVALID_ARGUMENT;
    }

    if (body != NULL)  memcpy(buffer + content, body, body_len);

    int total = 0;
    // Send the entire buffer to the connection
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
