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
    // Checking the validity of the pointers
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    size_t max_files = imgfs_file->header.max_files;
    size_t num_files = imgfs_file->header.nb_files;
    bool hasDuplicate = 0;
    if (index >= max_files || !imgfs_file->metadata[index].is_valid) {
        return ERR_IMAGE_NOT_FOUND;
    }
    struct img_metadata *image_i = &imgfs_file->metadata[index];
    // i is the number of images checked
    size_t i =0;
    // j is the number of valid images
    size_t j =0;
    while (j<num_files && i<max_files) {
        // Check that image is valid, and it is not the same as the one at position index
        if (imgfs_file->metadata[i].is_valid && i!=index) {
            // Image ID should be unique for each index
            if (strncmp(imgfs_file->metadata[i].img_id, image_i->img_id, 127) == 0) {
                return ERR_DUPLICATE_ID;
            }
            // Same SHA is equivalent to same image content
            if (memcmp(imgfs_file->metadata[i].SHA, image_i->SHA, SHA256_DIGEST_LENGTH) == 0) {
                for (int z = 0; z < NB_RES; z++) {
                    // Modify the metadata at the index position, to reference the attributes of the copy found
                    image_i->offset[z] = imgfs_file->metadata[i].offset[z];
                }
                hasDuplicate = 1;
            }
            j++;
        }
        i++;
    }
    // ORIG_RES offset set to 0 if image at position index has no duplicate content
    if (!hasDuplicate) {
        image_i->offset[ORIG_RES] = 0;
    }
    return ERR_NONE;
}
