#include "imgfs.h"
#include "util.h"
#include <stdio.h>         // for printf
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

        // Print metadata of all valid images
        for (int i = 0; i < (int)(imgfs_file->header.max_files); i++) {
            if (imgfs_file->metadata[i].is_valid == NON_EMPTY) {
                print_metadata(&(imgfs_file->metadata[i]));
                valid_images++;
            }
        }

        // Or when no valid images
        if (valid_images == 0) {
            printf("<< empty imgFS >>\n");
        }
    } else if (output_mode == JSON) {
        M_REQUIRE_NON_NULL(json);
        TO_BE_IMPLEMENTED();
    } else {
        // Output mode should only be JSON or STDOUT
        printf("Invalid output mode\n");
    }

    return ERR_NONE;
}
