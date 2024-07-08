
#include "imgfs.h"
#include "error.h"
#include <stdlib.h>  // for malloc, free
#include <string.h>  // for memcpy

int do_read(const char* img_id, int resolution, char** image_buffer, uint32_t* image_size, struct imgfs_file* imgfs_file)
{
    //Checking the validity of the parameters
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);

    // Find the image in metadata
    int i = 0;
    int j = 0;
    int index = -1;
    while (j<imgfs_file->header.nb_files && i < imgfs_file->header.max_files) {
        if (imgfs_file->metadata[i].is_valid) {
            if(strcmp(imgfs_file->metadata[i].img_id, img_id) == 0) {
                index = i;
                break;
            }
            j++;
        }
        i++;
    }

    if (index == -1) {
        return ERR_IMAGE_NOT_FOUND;
    }

    if (imgfs_file->metadata[index].size[resolution] == 0) {
        if (resolution != ORIG_RES) {
            int err = lazily_resize(resolution, imgfs_file, index);
            if (err != ERR_NONE) return err;
        }
    }

    // Set image size for output
    *image_size = imgfs_file->metadata[index].size[resolution];

    // Allocate memory for the image buffer
    *image_buffer = malloc(*image_size);
    if (*image_buffer == NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    // Position file pointer and read the image
    fseek(imgfs_file->file, imgfs_file->metadata[index].offset[resolution], SEEK_SET);
    if (fread(*image_buffer, 1, *image_size, imgfs_file->file) != *image_size) {
        free(*image_buffer);
        *image_buffer = NULL;
        return ERR_IO;
    }

    return ERR_NONE;
}
