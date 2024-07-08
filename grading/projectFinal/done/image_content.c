
#include "imgfs.h"
#include "image_content.h"
#include "error.h"
#include <vips/vips.h>
#include <stdlib.h>

// Helper function to free memory
// grader: useless wrapper to free (-1)
void freeMemory(unsigned char* buffer)
{
    free(buffer);
    // grader: buffer is local, hence setting it to null does nothing
    // grader: this function should have been a macro
    buffer = NULL;
}

/**
 * @brief Lazily resizes an image to the requested resolution.
 *
 * @param resolution the resolution to which the image should be resized
 * @param imgfs_file pointer to the imgFS file structure
 * @param index index of the image in the metadata
 * @return an error code indicating the success or failure of the operation.
 */
int lazily_resize(int resolution, struct imgfs_file* imgfs_file, size_t index)
{
    // Check the validity  of arguments
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    if (index >= imgfs_file->header.max_files || imgfs_file->metadata[index].is_valid == 0) {
        return ERR_INVALID_IMGID;
    }

    if (resolution != THUMB_RES && resolution != SMALL_RES && resolution != ORIG_RES) {
        return ERR_RESOLUTIONS;
    }

    // If the requested image already exists in the corresponding resolution, do nothing;
    if (resolution == ORIG_RES) return ERR_NONE;

    // Check if the image already exists in the requested resolution
    if (imgfs_file->metadata[index].size[resolution] != 0) return ERR_NONE;


    // Read the original image from disk into buffer and allocate space for buffer
    VipsImage *original_image;
    unsigned char *buffer = malloc(imgfs_file->metadata[index].size[ORIG_RES]);
    if (buffer == NULL) {
        freeMemory(buffer);
        return ERR_OUT_OF_MEMORY;
    }
    //Seek offset at which image is stored
    if (fseek(imgfs_file->file,(long)imgfs_file->metadata[index].offset[ORIG_RES],SEEK_SET) != ERR_NONE) {
        freeMemory(buffer);
        return ERR_IO;
    }
    if (fread(buffer,imgfs_file->metadata[index].size[ORIG_RES],1,imgfs_file->file) != 1) {
        freeMemory(buffer);
        return ERR_IO;
    }
    //Create VipsImage from buffer
    if (vips_jpegload_buffer(buffer,imgfs_file->metadata[index].size[ORIG_RES],&original_image, NULL)!=ERR_NONE) {
        freeMemory(buffer);
        return ERR_IO;
    };



    // Resize the image to the requested resolution
    VipsImage *resized_image = NULL;
    if (vips_thumbnail_image(original_image, &resized_image, imgfs_file->header.resized_res[2*resolution], "height",
                             imgfs_file->header.resized_res[(2*resolution) + 1],NULL) != ERR_NONE) {
        freeMemory(buffer);
        g_object_unref(original_image);
        return ERR_IMGLIB;
    }

    // Save the resized image to a buffer
    unsigned char *buffer2 = NULL;
    size_t buffer_size = 0;

    if (vips_jpegsave_buffer(resized_image, (void**)&buffer2, (size_t *)&buffer_size, NULL) != 0) {
        freeMemory(buffer);
        g_object_unref(original_image);
        g_object_unref(resized_image);
        return ERR_IMGLIB;
    }

    // Clean up resources
    g_object_unref(original_image);
    g_object_unref(resized_image);
    freeMemory(buffer);

    // Append the buffer to the end of imgFS file
    if (fseek(imgfs_file->file, 0, SEEK_END) != ERR_NONE) {
        return ERR_IO;
    }
    // Update metadata in memory and on disk
    imgfs_file->metadata[index].size[resolution] = buffer_size;
    imgfs_file->metadata[index].offset[resolution] = (uint64_t) ftell(imgfs_file->file);
    if (fwrite(buffer2, buffer_size, 1, imgfs_file->file) != 1) {
        freeMemory(buffer2);
        return ERR_IO;
    }

    freeMemory(buffer2);

    if (fseek(imgfs_file->file, (long)(sizeof(struct imgfs_header) + (index * sizeof(struct img_metadata))), SEEK_SET) != 0) {
        return ERR_IO;
    }
    if (fwrite(&imgfs_file->metadata[index], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    return ERR_NONE;
}


/**
 * @brief Gets the resolution of an image.
 *
 * @param height Where to put the calculated image height.
 * @param width Where to put the calculated image width.
 * @param image_buffer The image content.
 * @param image_size The size of the image (size of image_buffer).
 * @return Some error code. 0 if no error.
 */

int get_resolution(uint32_t *height, uint32_t *width,
                   const char *image_buffer, size_t image_size)
{
    M_REQUIRE_NON_NULL(height);
    M_REQUIRE_NON_NULL(width);
    M_REQUIRE_NON_NULL(image_buffer);

    VipsImage *original = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    const int err = vips_jpegload_buffer((void *) image_buffer, image_size,
                                         &original, NULL);
#pragma GCC diagnostic pop
    if (err != ERR_NONE) return ERR_IMGLIB;

    *height = (uint32_t) vips_image_get_height(original);
    *width = (uint32_t) vips_image_get_width(original);

    g_object_unref(VIPS_OBJECT(original));
    return ERR_NONE;
}
