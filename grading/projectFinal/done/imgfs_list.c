#include "imgfs.h"
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>


/**
 * @brief Displays (on stdout) imgFS metadata.
 *
 * @param imgfs_file In memory structure with header and metadata.
 * @param output_mode What style to use for displaying infos.
 * @param json A pointer to a string containing the list in JSON format if output_mode is JSON.
 *      It will be dynamically allocated by the function. Ignored for other output modes.
 * @return some error code.
 */
int do_list(const struct imgfs_file *imgfs_file,
            enum do_list_mode output_mode, char **json)
{
    // Checking the validity of the pointers
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    if (output_mode == STDOUT) {
        // Print contents of the header
        print_header(&(imgfs_file->header));
        int valid_images = 0;
        int i = 0;
        // Print metadata of all valid images
        while (i < (int)(imgfs_file->header.max_files) && valid_images < (int)(imgfs_file->header.nb_files)) {
            if (imgfs_file->metadata[i].is_valid == NON_EMPTY) {
                print_metadata(&(imgfs_file->metadata[i]));
                valid_images++;
            }
            i++;
        }

        // Or when no valid images
        if (valid_images == 0) {
            printf("<< empty imgFS >>\n");
        }
    } else if (output_mode == JSON) {
        M_REQUIRE_NON_NULL(json);
        // Create a JSON object to store imgids
        struct json_object* obj = json_object_new_object();
        if (obj == NULL) {
            perror("json_object_new_object");
            return ERR_RUNTIME;
        }
        // Create an array to store imgids
        struct json_object* imgids = json_object_new_array_ext(imgfs_file->header.nb_files);
        if (imgids == NULL) {
            perror("json_object_new_array_ext");
            json_object_put(obj);
            return ERR_RUNTIME;
        }
        // Add all valid imgids to the array
        int i = 0;
        int valid_images = 0;
        while (i < (int)(imgfs_file->header.max_files) && valid_images < (int)(imgfs_file->header.nb_files)) {
            if (imgfs_file->metadata[i].is_valid == NON_EMPTY) {
                struct json_object* imgid = json_object_new_string(imgfs_file->metadata[i].img_id);
                if (imgid == NULL) {
                    perror("json_object_new_string");
                    json_object_put(obj);
                    json_object_put(imgids);
                    return ERR_RUNTIME;
                }
                int err = json_object_array_add(imgids, imgid);
                if (err != 0) {
                    perror("json_object_array_add");
                    json_object_put(obj);
                    json_object_put(imgids);
                    return ERR_RUNTIME;
                }
                valid_images++;
            }
            i++;
        }
        // Add the array to the JSON object
        int err2 = json_object_object_add(obj, "Images", imgids);
        if (err2 != 0) {
            perror("json_object_object_add");
            json_object_put(obj);
            json_object_put(imgids);
            return ERR_RUNTIME;
        }
        // Convert the JSON object to a string
        const char* jsonstring = json_object_to_json_string(obj);
        *json = (const char*)malloc(strlen(jsonstring) + 1);
        if (*json == NULL) {
            perror("malloc");
            json_object_put(obj);
            return ERR_OUT_OF_MEMORY;
        }

        strcpy(*json, jsonstring);
        // Free the JSON object
        json_object_put(obj);
        return ERR_NONE;

    } else {
        // Output mode should only be JSON or STDOUT
        perror("Invalid output mode\n");
        return ERR_INVALID_ARGUMENT;
    }

}
