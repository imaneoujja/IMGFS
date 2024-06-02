/*
 * @file imgfs_server_service.c
 * @brief ImgFS server part, bridge between HTTP server layer and ImgFS library
 *
 * @author Marta Adarve de Leon & Imane Oujja
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint16_t
#include <pthread.h>
#include "error.h"
#include "util.h" // atouint16
#include "imgfs.h"
#include "http_net.h"
#include "imgfs_server_service.h"

// Main in-memory structure for imgFS
static struct imgfs_file fs_file;
static uint16_t server_port = DEFAULT_LISTENING_PORT;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define URI_ROOT "/imgfs"
#define DEFAULT_LISTENING_PORT 8000

/***********************//*
 * Startup function. Create imgFS file and load in-memory structure.
 * Pass the imgFS file name as argv[1] and optionally port number as argv[2]
 ******************** */
int server_startup(int argc, char **argv) {
    M_REQUIRE_NON_NULL(argv);
    if (pthread_mutex_init(&mutex, NULL) != ERR_NONE){
        perror("pthread_mutex_init");
        return ERR_RUNTIME;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <imgFS_filename> [port]\n", argv[0]);
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    if (pthread_mutex_lock(&mutex) != ERR_NONE){
        perror("pthread_mutex_lock");
        return ERR_RUNTIME;
    }
    if (do_open(argv[1], "rb+", &fs_file) != ERR_NONE) {
        fprintf(stderr, "Failed to open ImgFS file: %s\n", argv[1]);
        return ERR_IO;
    }
    if (pthread_mutex_unlock(&mutex) != ERR_NONE){
        perror("pthread_mutex_lock");
        return ERR_RUNTIME;
    }
    print_header(&fs_file.header);

    if (argc > 2) {
        server_port = atouint16(argv[2]);
        if (server_port == 0) server_port = DEFAULT_LISTENING_PORT;
    }

    if (http_init(server_port, handle_http_message) < 0) {
        fprintf(stderr, "HTTP initialization failed on port %u\n", server_port);
        return ERR_IO;
    }

    printf("ImgFS server started on http://localhost:%u\n", server_port);
    return ERR_NONE;
}

/*************************
 * Shutdown function. Free the structures and close the file.
 ******************** */
void server_shutdown (void)
{
    fprintf(stderr, "Shutting down...\n");
    pthread_mutex_lock(&mutex);
    do_close(&fs_file);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    http_close();
}

/************************
 * Sends error message.
 ******************** */
static int reply_error_msg(int connection, int error)
{
#define ERR_MSG_SIZE 256
    char err_msg[ERR_MSG_SIZE]; // enough for any reasonable err_msg
    if (snprintf(err_msg, ERR_MSG_SIZE, "Error: %s\n", ERR_MSG(error)) < 0) {
        fprintf(stderr, "reply_error_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "500 Internal Server Error", "",
                      err_msg, strlen(err_msg));
}

/************************
 * Sends 302 OK message.
 ******************** */
static int reply_302_msg(int connection)
{
    char location[ERR_MSG_SIZE];
    if (snprintf(location, ERR_MSG_SIZE, "Location: http://localhost:%d/" BASE_FILE HTTP_LINE_DELIM,
                 server_port) < 0) {
        fprintf(stderr, "reply_302_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "302 Found", location, "", 0);
}

/************************
 * Simple handling of http message. UPDATED IN WEEK 13
 ******************** */
int handle_http_message(struct http_message* msg, int sockfd)
{
    M_REQUIRE_NON_NULL(msg);
    debug_printf("handle_http_message() on connection %d. URI: %.*s\n",sockfd,(int) msg->uri.len, msg->uri.val);
    if (http_match_verb(&msg->uri, "/") || http_match_uri(msg, "/index.html")) {
        return http_serve_file(sockfd, BASE_FILE);
    }

    if (http_match_uri(msg, URI_ROOT "/list") ){
        return handle_list_call(sockfd);
    }
    if(   (http_match_uri(msg, URI_ROOT "/insert")&& http_match_verb(&msg->method, "POST")) ){
        return handle_insert_call(msg, sockfd);
    }
    if(http_match_uri(msg, URI_ROOT "/read")){
        return handle_read_call(msg,sockfd);
    }
    if(   http_match_uri(msg, URI_ROOT "/delete")){
        return handle_delete_call(msg,sockfd);
    }
    return reply_error_msg(sockfd, ERR_INVALID_COMMAND);
}


/************************
 * Handling list calls
 ******************** */
int handle_list_call(int connection) {
    // Set the output mode to JSON
    enum do_list_mode output_mode = JSON;
    char* json;

    // List using the do_list function
    int error = do_list(&fs_file, output_mode, &json);
    if (error != ERR_NONE) return reply_error_msg(connection, error);
    

    // Send the HTTP reply with the JSON content
    int result = http_reply(connection, HTTP_OK, "Content-Type: application/json\r\n", json, strlen(json));
    free(json);
    return result;
}


/************************
 * Handling delete calls
 ******************** */
int handle_delete_call(struct http_message *msg, int sockfd) {
    char img_id[MAX_IMG_ID];

    // Get the img_id parameter from the URI
    int err = http_get_var(&msg->uri, "img_id", img_id, sizeof(img_id));
    if (err <= 0) {
        // If the img_id is not provided, reply with an error message
        return reply_error_msg(sockfd, ERR_NOT_ENOUGH_ARGUMENTS);
    }

    // Perform the delete operation
    int ret = do_delete(img_id, &fs_file);
    if (ret != ERR_NONE) {
        // If there is an error during deletion, reply with the error message
        return reply_error_msg(sockfd, ret);
    }

    // Reply with a 302 redirect message on success
    return reply_302_msg(sockfd);
}

/************************
 * Handling read calls
 ******************** */
int handle_read_call(struct http_message *msg, int sockfd) {
    char img_id[MAX_IMG_ID + 1] = {0};
    char res[6] = {0};
    int err;

    // Get the img_id parameter from the URI
    if ((err = http_get_var(&msg->uri, "img_id", img_id, MAX_IMG_ID)) <= 0) {
        return reply_error_msg(sockfd, ERR_NOT_ENOUGH_ARGUMENTS);
    }

    // Get the res parameter from the URI
    if ((err = http_get_var(&msg->uri, "res", res, sizeof(res))) <= 0) {
        if (err == 0) {
            return reply_error_msg(sockfd, ERR_NOT_ENOUGH_ARGUMENTS);
        }
        return reply_error_msg(sockfd, ERR_RESOLUTIONS);
    }

    // Convert the resolution string to an integer
    int resolution = resolution_atoi(res);
    if (resolution == -1) return reply_error_msg(sockfd, ERR_RESOLUTIONS);
    

    // Prepare to read the image
    char *image_buffer = NULL;
    uint32_t image_size = 0;
    int error = do_read(img_id, resolution, &image_buffer, &image_size, &fs_file);

    if (error != ERR_NONE) return reply_error_msg(sockfd, error);


    // Send the response with the image
    int result = http_reply(sockfd, HTTP_OK, "Content-Type: image/jpeg\r\n", image_buffer, image_size);

    free(image_buffer);
    return result;

}

// Function to handle insert calls
int handle_insert_call(struct http_message *msg, int sockfd) {
    char img_id[MAX_IMG_ID + 1];

    // Get the name parameter from the URI
    int err = http_get_var(&msg->uri, "name", img_id, MAX_IMG_ID);

    // Check if the body is empty or the name parameter is not provided
    if (msg->body.len == 0 || err <= 0)  return reply_error_msg(sockfd, ERR_NOT_ENOUGH_ARGUMENTS);
    

    // Allocate memory for the image buffer
    char *image_buffer = calloc(1, msg->body.len);
    if (image_buffer == NULL) return reply_error_msg(sockfd, ERR_OUT_OF_MEMORY);
    

    // Copy the image data to the buffer
    memcpy(image_buffer, msg->body.val, msg->body.len);

    // Perform the insert operation
    int ret = do_insert(image_buffer, msg->body.len, img_id, &fs_file);
    free(image_buffer);

    if (ret != ERR_NONE) return reply_error_msg(sockfd, ret);
    
    return reply_302_msg(sockfd);
}