#include "imgfs.h"
#include "error.h"
#include <string.h>

/**
 * @brief Deletes an image with the specified img_id from the imgFS file by invalidating its reference.
 *
 * @param img_id identifier of the image to be deleted
 * @param imgfs_file pointer to the imgFS file structure
 * @return an error code indicating the success or failure of the operation.
 */
int do_delete(const char* img_id, struct imgfs_file* imgfs_file)
{


    // Checking the validity of the parameters
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(img_id);



    int img_found = -1;
    uint32_t i = 0;
    uint32_t j = 0;
    // Find image reference in the metadata
    while (j<imgfs_file->header.nb_files && i < imgfs_file->header.max_files) {
        if (imgfs_file->metadata[i].is_valid == NON_EMPTY) {
            ++j;
            if (strcmp(img_id, imgfs_file->metadata[i].img_id) == 0) {
                img_found = i;
                break;
            }
        }
        ++i;
    }

    // Image reference does not exist
    if (img_found == -1) return ERR_IMAGE_NOT_FOUND;

    // Image deleted by invalidating the reference
    imgfs_file->metadata[img_found].is_valid = EMPTY;

    // Set the position to write the updated metadata to the file
    fseek(imgfs_file->file, (long)(sizeof(struct imgfs_header) + (unsigned long)(img_found) * sizeof(struct img_metadata)), SEEK_SET);
    if (fwrite(&imgfs_file->metadata[img_found], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    // Move the file pointer to the beginning of the file
    rewind(imgfs_file->file);

    // Update the header
    imgfs_file->header.nb_files--;
    imgfs_file->header.version++;

    // Write updated header to disk
    if (fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    // All good
    return ERR_NONE;
}