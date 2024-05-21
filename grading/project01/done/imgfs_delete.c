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

    // grader: the case nb_files == 0 is missing (-1)

    // grader: avoid mixing error codes with values
    int img_found = ERR_IMAGE_NOT_FOUND;

    // Find image reference in the metadata
    // grader: max_files might not fit in an int
    for (int i = 0; i < (int)imgfs_file->header.max_files; ++i) {
        if (imgfs_file->metadata[i].is_valid == NON_EMPTY && strcmp(imgfs_file->metadata[i].img_id, img_id) == 0) {
            img_found = i;
            break;
        }
    }

    // Image reference does not exist
    if (img_found == ERR_IMAGE_NOT_FOUND) return ERR_IMAGE_NOT_FOUND;

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
