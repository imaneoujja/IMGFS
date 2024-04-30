
#include "imgfs.h"
#include "image_content.h"
#include "error.h"
#include <vips/vips.h>
#include <stdlib.h>

int lazily_resize(int resolution, struct imgfs_file* imgfs_file, size_t index) {
    // Check the legitimacy of arguments
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    if (index >= imgfs_file->header.max_files || index<0 || imgfs_file->metadata[index].is_valid == 0) {
        return ERR_INVALID_IMGID;
    }
    if (resolution != THUMB_RES && resolution != SMALL_RES && resolution != ORIG_RES) {
        return ERR_INVALID_ARGUMENT;
    }

    // If the requested image already exists in the corresponding resolution, do nothing;
    if (resolution == ORIG_RES) {
        return ERR_NONE;
    }


    // Check if the image already exists in the requested resolution
    if (imgfs_file->metadata[index].size[resolution] != 0) {
        return ERR_NONE;
    }

    // Read the original image from disk into buffer
    VipsImage *original_image;
    unsigned char *buffer = malloc(imgfs_file->metadata[index].size[ORIG_RES]);
    if (buffer == NULL){
        return ERR_OUT_OF_MEMORY;
    }
    if (fseek(imgfs_file->file,imgfs_file->metadata[index].offset[ORIG_RES],SEEK_SET) != ERR_NONE){
        return ERR_IO;
    }
    if (fread(buffer,sizeof(unsigned char),imgfs_file->metadata[index].size[ORIG_RES],imgfs_file->file) != imgfs_file->metadata[index].size[ORIG_RES]){
        return ERR_IO;
    }
    if (vips_jpegload_buffer(buffer,imgfs_file->metadata[index].size[ORIG_RES],&original_image, NULL)!=ERR_NONE){
        return ERR_IO;
    };



    // Resize the image to the requested resolution
    VipsImage *resized_image = NULL;
    if (resolution == THUMB_RES) {
        // Resize to thumbnail resolution
        if (vips_thumbnail_image(original_image, &resized_image, imgfs_file->header.resized_res[THUMB_RES], "height",
                                 imgfs_file->header.resized_res[THUMB_RES + 1],NULL) != ERR_NONE) {
            g_object_unref(original_image);
            return ERR_IMGLIB;
        }
    } else {
        // Resize to small resolution
        if (vips_thumbnail_image(original_image, &resized_image, imgfs_file->header.resized_res[SMALL_RES], "height",
                                 imgfs_file->header.resized_res[SMALL_RES + 1],NULL)!= ERR_NONE) {
            g_object_unref(original_image);
            return ERR_IMGLIB;
        }
    }

    // Save the resized image to a buffer
    unsigned char *buffer2 = NULL;
    size_t buffer_size = 0;

    if (vips_jpegsave_buffer(resized_image, (void**)&buffer2, &buffer_size , NULL) != 0) {
        g_object_unref(original_image);
        g_object_unref(resized_image);
        return ERR_IMGLIB;
    }

    // Clean up resources
    g_object_unref(original_image);
    g_object_unref(resized_image);
    free(buffer);
    buffer = NULL;

    // Append the buffer to the imgFS file
    if (fwrite(buffer2, sizeof(unsigned char), buffer_size, imgfs_file->file) != buffer_size) {
        return ERR_IO;
    }

    // Update metadata in memory and on disk
    imgfs_file->metadata[index].size[resolution] = buffer_size;
    imgfs_file->metadata[index].offset[resolution] = ftell(imgfs_file->file);

    if (fseek(imgfs_file->file, sizeof(struct imgfs_header) + (index * sizeof(struct img_metadata)), SEEK_SET) != 0) {
        return ERR_IO;
    }
    if (fwrite(&imgfs_file->metadata[index], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }


    return ERR_NONE;
}