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
    size_t num_files = imgfs_file->header.nb_files;
    bool hasDuplicate = 0;
    if (index > num_files) {
        return ERR_IMAGE_NOT_FOUND;
    }
    struct img_metadata *image_i = &imgfs_file->metadata[index];
    // grader: incorrect condition, should be i < max_files (-1)
    for (size_t i =0; i<num_files; i++) {
        // Check that image is valid, and it is not the same as the one at position index
        if (imgfs_file->metadata[i].is_valid && i!=index) {
            // Image ID should be unique for each index
	    // grader: use MAX_IMG_ID instead of 127
            if (strncmp(imgfs_file->metadata[i].img_id, image_i->img_id, 127) == 0) {
                return ERR_DUPLICATE_ID;
            }
            // Same SHA is equivalent to same image content
            if (memcmp(imgfs_file->metadata[i].SHA, image_i->SHA, SHA256_DIGEST_LENGTH) == 0) {
                for (int j = 0; j < NB_RES; j++) {
                    // Modify the metadata at the index position, to reference the attributes of the copy found
                    image_i->offset[j] = imgfs_file->metadata[i].offset[j];
		    // grader: size is not updated (-0.5)
                }
                hasDuplicate = 1;
            }
        }
    }
    // ORIG_RES offset set to 0 if image at position index has no duplicate content
    if (!hasDuplicate) {
        image_i->offset[ORIG_RES] = 0;
    }
    return ERR_NONE;
}