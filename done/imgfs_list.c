#include "imgfs.h"
#include "util.h"

#include <inttypes.h>      // for PRIxN macros
#include <openssl/sha.h>   // for SHA256_DIGEST_LENGTH
#include <stdint.h>        // for uint8_t
#include <stdio.h>         // for sprintf
#include <stdlib.h>        // for calloc
#include <string.h>        // for strcmp

/**
 * @brief Displays (on stdout) imgFS metadata.
 *
 * @param imgfs_file In memory structure with header and metadata.
 * @param output_mode What style to use for displaying infos.
 * @param json A pointer to a string containing the list in JSON format if output_mode is JSON.
 *      It will be dynamically allocated by the function. Ignored for other output modes.
 * @return some error code.
 */
int do_list(const struct imgfs_file* imgfs_file,
            enum do_list_mode output_mode, char** json){
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    if (output_mode == STDOUT){
        print_header(&imgfs_file->header);
        if (imgfs_file->header.nb_files == 0){
            printf("<< empty imgFS >>\n");
        }
        else{
            for(int i = 0; i<imgfs_file->header.nb_files;i++){
                if (imgfs_file->metadata[i].is_valid == NON_EMPTY){
                    print_metadata(&(imgfs_file->metadata[i]));
                }
            }

        }
    }
    else{
        M_REQUIRE_NON_NULL(json);
        TO_BE_IMPLEMENTED();
    }
    return ERR_NONE;
}