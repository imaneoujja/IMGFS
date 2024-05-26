#include "http_prot.h"
#include <string.h>
#include "imgfs.h"
#include "error.h"
#ifdef IN_CS202_UNIT_TEST
#define static_unless_test
#else
#define static_unless_test static
#endif

/**
 * @brief Checks whether the message URI starts with the provided target_uri.
 *
 * Returns: 1 if it does, 0 if it does not.
 *
 */
int http_match_uri(const struct http_message *message, const char *target_uri){
    M_REQUIRE_NON_NULL(message);
    M_REQUIRE_NON_NULL(message->uri.val);
    M_REQUIRE_NON_NULL(target_uri);
    return strncmp(message->uri.val, target_uri, strlen(target_uri)) == 0;
}

/**
 * @brief Compare method with verb and return 1 if they are equal, 0 otherwise
 */
int http_match_verb(const struct http_string* method, const char* verb){
    M_REQUIRE_NON_NULL(method);
    M_REQUIRE_NON_NULL(verb);
    M_REQUIRE_NON_NULL(method->val);
    return (method->len == strlen(verb) && strncmp(method->val, verb, method->len) == 0);
}

/**
 * @brief Writes the value of parameter name from URL in message to buffer out.
 *
 * Return the length of the value.
 * 0 or negative return values indicate an error.
 */
int http_get_var(const struct http_string* url, const char* name, char* out, size_t out_len){
    M_REQUIRE_NON_NULL(url);
    M_REQUIRE_NON_NULL(url->val);
    M_REQUIRE_NON_NULL(name);
    M_REQUIRE_NON_NULL(out);
    // Creating a new string with the name of the parameter and '='
    char param[strlen(name) + 2];
    strncpy(param, name, strlen(name));
    param[strlen(name)] = '\0';
    strncat(param, "=", 1);
    // Searching for the parameter in the URL
    const char* start = strstr(url->val, param);
    if (start == NULL) {
        return 0;
    }
    start += strlen(param);
    // Look if there is any '&' somewhere after that string and if yes consider the position of this '&' as the end of the value
    const char* end = strchr(start, '&');
    // Otherwise consider the end of the URL as the end of the value;
    if (end == NULL) {
        end = url->val + url->len;
    }
    // Copy the value in the out buffer
    size_t len = end - start;
    if (len > out_len) {
        return ERR_RUNTIME;
    }
    strncpy(out, start, len);
    out[len] = '\0';
    return len;

}

/**
 * @brief Accepts a potentially partial TCP stream and parses an HTTP message.
 *
 * Assumes that all characters of stream that are not filled by reading are set to 0.
 *
 * Places the complete HTTP message in out.
 * Also writes the content of header "Content Length" to content_len upon parsing the header in the stream.
 * content_len can be used by the caller to allocate memory to receive the whole HTTP message.
 *
 * Returns:
 *  a negative int if there was an error
 *  0 if the message has not been received completely (partial treatment) or an error occurred
 *  1 if the message was fully received and parsed
 */
int http_parse_message(const char *stream, size_t bytes_received, struct http_message *out, int *content_len) {
    M_REQUIRE_NON_NULL(stream);
    M_REQUIRE_NON_NULL(out);
    M_REQUIRE_NON_NULL(content_len);

    // Initialize the out structure and content_len
    memset(out, 0, sizeof(struct http_message));
    *content_len = 0;
    // Check headers have been completely received
    const char *header_end = strstr(stream, HTTP_HDR_END_DELIM);
    if (!header_end) {
        return 0;
    }
    const char *start = stream;
    // Parse the first token
    stream = get_next_token(stream, " ", &out->method);
    if (stream == NULL) {
        return 0;
    }
    // Parse the second token
    stream = get_next_token(stream, " ", &out->uri);
    if (stream == NULL) {
        return 0;
    }
    // Check third token
    stream = get_next_token(stream, HTTP_LINE_DELIM, NULL);
    if (stream == NULL) {
        return 0;
    }
    // Parse all the key-value pairs
    stream = http_parse_headers(stream, out);
    if (stream == NULL) {
        return 0;
    }
    // Get the "Content-Length" value from the parsed header lines
for (size_t i = 0; i < out->num_headers; ++i) {
        if (strncmp(out->headers[i].key.val, "Content-Length", strlen("Content-Length")) == 0) {
            *content_len = atoi(out->headers[i].value.val);
            break;
        }
    }
    // If it's present and not 0, store the body
    if (content_len !=NULL && *content_len != 0 ) {
        out->body.val = stream;
        out->body.len = bytes_received - (stream-start);
        return out->body.len == *content_len;
    }
    return 1;


}


/**
 * @brief Extract the first substring (= prefix) of a the string before some delimiter:
 *
 * Returns:
 * - the pointer to the first character after the delimiter
 */
static_unless_test const char* get_next_token(const char* message, const char* delimiter, struct http_string* output){
    M_REQUIRE_NON_NULL(message);
    M_REQUIRE_NON_NULL(delimiter);
    // Look for the delimiter in the message
    const char* end = strstr(message, delimiter);
    if (end == NULL) {
        return NULL;
    }
    // Copy the substring in the output buffer except if output is null
    if (output != NULL) {
        output->len = end - message;
        output->val = message;
    }
    // Third token will be skipped but must be checked
    else{
        assert(strncmp(message, "HTTP/1.1", strlen("HTTP/1.1")) == 0);
    }


    return end + strlen(delimiter);
}

/**
 * @brief Fill all headers key-value pairs of output
 *
 * Returns:
 * - the position right after the last header line
 */
static_unless_test const char* http_parse_headers(const char* header_start, struct http_message* output) {
    // Check that the line delimiter is twice the line delimiter
    _Static_assert(strcmp(HTTP_HDR_END_DELIM, HTTP_LINE_DELIM HTTP_LINE_DELIM) == 0,
                   "HTTP_HDR_END_DELIM is not twice HTTP_LINE_DELIM");
    M_REQUIRE_NON_NULL(header_start);
    M_REQUIRE_NON_NULL(output);
    // Parse all the headers
    while (strncmp(header_start, HTTP_LINE_DELIM, strlen(HTTP_LINE_DELIM)) != 0) {
        // Check that array is not full
        if (output->num_headers >= MAX_HEADERS) {
            return ERR_OUT_OF_MEMORY;
        }
        struct http_header* header = &output->headers[output->num_headers];
        // Parse the key
        header_start = get_next_token(header_start, HTTP_HDR_KV_DELIM , &header->key);
        if (header_start == NULL) {
            return NULL;
        }
        // Parse the value
        header_start = get_next_token(header_start, HTTP_LINE_DELIM, &header->value);
        if (header_start == NULL) {
            return NULL;
        }
        output->num_headers++;
    }
    return header_start + strlen(HTTP_LINE_DELIM);

}