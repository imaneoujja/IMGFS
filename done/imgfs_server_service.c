/*
 * @file imgfs_server_service.c
 * @brief ImgFS server part, bridge between HTTP server layer and ImgFS library
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint16_t

#include "error.h"
#include "util.h" // atouint16
#include "imgfs.h"
#include "http_net.h"
#include "imgfs_server_service.h"

// Main in-memory structure for imgFS
static struct imgfs_file fs_file;
static uint16_t server_port = DEFAULT_LISTENING_PORT;

#define URI_ROOT "/imgfs"
#define DEFAULT_LISTENING_PORT 8000

/***********************//*
 * Startup function. Create imgFS file and load in-memory structure.
 * Pass the imgFS file name as argv[1] and optionally port number as argv[2]
 ******************** */
int server_startup(int argc, char **argv) {
    M_REQUIRE_NON_NULL(argv);

    if (argc < 3) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    int err = do_open(argv[1], "rb+", &fs_file);
    if (err != ERR_NONE) {
        return err;
    }

    print_header(&fs_file.header);

    if (argc > 2) {
        err = atouint16(argv[2]);
        if (err >= 1024) {
            server_port = err;
        }
    } else {
        server_port = DEFAULT_LISTENING_PORT;
    }

    int server_socket = http_init(server_port, handle_http_message);
    if (server_socket < 0) {
        http_close;
        return ERR_IO;
    }

    printf("ImgFS server started on http://localhost:%u\n", server_port);
    return ERR_NONE;
}

/***********************//*
 * Shutdown function. Free the structures and close the file.
 ******************** */
void server_shutdown(void) {
    fprintf(stderr, "Shutting down...\n");
    http_close();
    do_close(&fs_file);
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
 * Simple handling of http message. TO BE UPDATED WEEK 13
 ******************** */
int handle_http_message(struct http_message* msg, int sockfd)
{
    M_REQUIRE_NON_NULL(msg);    debug_printf("handle_http_message() on connection %d. URI: %.*s\n",
                 sockfd,                 (int) msg->uri.len, msg->uri.val);
    if (http_match_verb(&msg->uri, "/") || http_match_uri(msg, "/index.html")) {
        return http_serve_file(sockfd, BASE_FILE);
    }

    if (http_match_uri(msg, URI_ROOT "/list") ){        return handle_list_call(sockfd);
    }        if(   (http_match_uri(msg, URI_ROOT "/insert")&& http_match_verb(&msg->method, "POST")) ){
        return handle_insert_call(msg, sockfd);         }
    if(http_match_uri(msg, URI_ROOT "/read")){        return handle_read_call(msg,sockfd);
    }         if(   http_match_uri(msg, URI_ROOT "/delete")){
        return handle_delete_call(msg,sockfd);    }
        return reply_error_msg(sockfd, ERR_INVALID_COMMAND);
}


int handle_list_call( int connection){   
    enum do_list_mode output_mode = JSON;
    char* json;
    // List using the do_list function
    int error = do_list(&fs_file, output_mode, &json);
    if (error != ERR_NONE) {
        return reply_error_msg(connection, error);
    }
    int result = http_reply(connection, "200 OK", "Content-Type: application/json\r\n", json, strlen(json));
    free(json); // Make sure to free the allocated JSON string
    return result;
}


int handle_delete_call(struct http_message *msg, int sockfd) { 
    char img_id[MAX_IMG_ID];  
    int err = http_get_var(&msg->uri, "img_id", img_id, sizeof(img_id)) ; 
    if (err<= 0) { 
        return reply_error_msg(sockfd, err); 
    }  
    int ret = do_delete("imgfs_file.imgfs", img_id);     if (ret != ERR_NONE) { 
        return reply_error_msg(sockfd, ret);     } 
     return reply_302_msg(sockfd); 
} 

int handle_read_call(struct http_message *msg, int sockfd) { 
    char img_id[MAX_IMG_ID + 1] = {0};    
    char res[6] = {0};
    int err;
    if (err = http_get_var(&msg->uri, "img_id", img_id, MAX_IMG_ID)<= 0) {
        return reply_error_msg(sockfd, err);    }
    if (http_get_var(&msg->uri, "res", res, sizeof(res) - 1)<=0) {
        return reply_error_msg(sockfd, ERR_INVALID_ARGUMENT);    }
    char *image_buffer = NULL;
    uint32_t image_size = 0;
    int error = do_read(img_id, resolution_atoi(res), &image_buffer, &image_size, &fs_file);    
    if (error != ERR_NONE) {
        return reply_error_msg(sockfd, error);    }
    int result = http_reply(sockfd, HTTP_OK, "Content-Type: image/jpeg\r\n", image_buffer, image_size);
    free(image_buffer);  // Make sure to free the image buffer after use    
    return result;

} 
int handle_insert_call(struct http_message *msg, int sockfd) { 
    char img_id[MAX_IMG_ID];    
    int err = http_get_var(&msg->uri, "name", img_id, sizeof(img_id));
     if (err<=0) { 
        return reply_error_msg(sockfd, err); 
    }  
    char *image_buffer = malloc(msg->body.len);    
     if (!image_buffer) { 
        return reply_error_msg(sockfd, ERR_OUT_OF_MEMORY);     } 
    memcpy(image_buffer, msg->body.val, msg->body.len);  
    int ret = do_insert("imgfs_file.imgfs", img_id, image_buffer, msg->body.len);     free(image_buffer); 
    if (ret != ERR_NONE) { 
        return reply_error_msg(sockfd, ret); 
    }  
    return reply_302_msg(sockfd); }

