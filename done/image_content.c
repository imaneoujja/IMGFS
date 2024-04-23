#include "imgfs.h"
#include "image_content.h"
#include "error.h"
#include <stdio.h> 


int lazily_resize(int resolution, struct imgfs_file* imgfs_file, size_t index) {
  
    if (resolution == ORIG_RES) {
        return ERR_NONE;
    }

    if (imgfs_file == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (index >= imgfs_file->header.max_files) {
        return ERR_INVALID_ARGUMENT;
    }

 
    if (imgfs_file->metadata[index].offset[resolution] != 0) {
        return ERR_NONE;
    }

 
    return ERR_NONE;
}