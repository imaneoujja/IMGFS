#include "imgfs.h"  // for struct imgfs_file
#include <string.h>
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
int do_name_and_content_dedup(struct imgfs_file* imgfs_file, uint32_t index)
{
    // Checking the validity of the parameters
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(imgfs_file->file);

    size_t max_files=imgfs_file->header.max_files;

    size_t num_files=imgfs_file->header.nb_files;

    if(index<0||index>=max_files) return ERR_IMAGE_NOT_FOUND;

    struct img_metadata* image_i = &imgfs_file->metadata[index];

    // Iterate through the metadata array to find duplicates
    for(int i=0; i < max_files; i++) {
        struct img_metadata* metadata = &imgfs_file->metadata[i];
        if(metadata->is_valid && i!=index) {
            if(strcmp(metadata->img_id,image_i->img_id)==0 ) return ERR_DUPLICATE_ID ;
            // Copy resolution, offset, and size information from the duplicate
            if(memcmp(metadata->SHA,image_i->SHA,SHA256_DIGEST_LENGTH)==0) {
                memcpy(image_i->orig_res, metadata->orig_res, sizeof(metadata->orig_res));
                memcpy(image_i->offset, metadata->offset, sizeof(metadata->offset));
                memcpy(image_i->size, metadata->size, sizeof(metadata->size));
                return ERR_NONE;
            }
        }
    }

    // No duplicate found, set the original resolution offset to 0
    image_i->offset[ORIG_RES]=0;
    return ERR_NONE;
}