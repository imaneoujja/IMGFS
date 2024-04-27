#include "imgfs.h"
#include "image_content.h"
#include "error.h"
#include <stdio.h> 


int lazily_resize(int resolution, struct imgfs_file* imgfs_file, size_t index) {
    M_REQUIRE_NON_NULL(imgfs_file);
    if (resolution != THUMB_RES && resolution != SMALL_RES) {
        return ERR_INVALID_IMGID;
    }

    if (index < 0 || index >= imgfs_file->header.max_files) {
        return ERR_INVALID_COMMAND;
    }
    if (imgfs_file->metadata[index].size[resolution] != 0) {
        return ERR_NONE;}


    char *original_image_data = imgfs_file->metadata[index].img_id;


    char *resized_image_data = "Resized Image Data";

 
    if (fseek(imgfs_file->file, 0, SEEK_END) != 0) {
        return ERR_IO;
    }
    if (fwrite(resized_image_data, sizeof(char), strlen(resized_image_data), imgfs_file->file) != strlen(resized_image_data)) {
        return ERR_IO;
    }

    imgfs_file->metadata[index].size[resolution] = strlen(resized_image_data);
    imgfs_file->metadata[index].offset[resolution] = ftell(imgfs_file->file);

    if (fseek(imgfs_file->file, sizeof(struct imgfs_header) + index * sizeof(struct img_metadata), SEEK_SET) != 0) {
        return ERR_IO;
    }
    if (fwrite(&imgfs_file->metadata[index], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    return ERR_NONE;
}