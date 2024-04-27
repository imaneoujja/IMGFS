#include "imgfs.h"  // for struct imgfs_file
#include <string.h>
#include <stdio.h>  // for FILE
#include <stdint.h> // for uint32_t
#define true 1
#define false 0
typedef int bool;

/**
 * @brief Does image deduplication.
 *
 * @param imgfs_file The main in-memory structure
 * @param index The order number in the metadata array
 * @return Some error code. 0 if no error.
 */
int do_name_and_content_dedup(struct imgfs_file* imgfs_file, uint32_t index){
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    size_t num_files = imgfs_file->header.nb_files;
    bool hasDuplicate = 0;
    if (index > num_files){
        return ERR_IMAGE_NOT_FOUND;
    }
    struct img_metadata *image_i = &imgfs_file->metadata[index];
    for (size_t i =0;i<num_files;i++){
        if (imgfs_file->metadata[i].is_valid && i!=index){
            if (strncmp(imgfs_file->metadata[i].img_id, image_i->img_id, 127) == 0){
                return ERR_DUPLICATE_ID;
            }
            if (memcmp(imgfs_file->metadata[i].SHA, image_i->SHA, SHA256_DIGEST_LENGTH) == 0){
                for (int j = 0; j < NB_RES; j++) {
                    image_i->offset[j] = imgfs_file->metadata[i].offset[j];
                }
                hasDuplicate = 1;
            }
        }
    }
    if (!hasDuplicate){
        image_i->offset[ORIG_RES] = 0;
    }
    return ERR_NONE;}
