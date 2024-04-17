#include "imgfs.h"

#include <stdlib.h>        // for calloc
#include "string.h"
/**
 * @brief Creates the imgFS called imgfs_filename. Writes the header and the
 *        preallocated empty metadata array to imgFS file.
 *
 * @param imgfs_filename Path to the imgFS file
 * @param imgfs_file In memory structure with header and metadata.
 */
int do_create(const char* imgfs_filename, struct imgfs_file* imgfs_file){
    M_REQUIRE_NON_NULL(imgfs_filename);
    M_REQUIRE_NON_NULL(imgfs_file);
    FILE *filePointer;
    strncpy(imgfs_file->header.name, CAT_TXT, sizeof(imgfs_file->header.name) - 1);
    imgfs_file->header.name[sizeof(imgfs_file->header.name) - 1] = '\0';

    filePointer = fopen(imgfs_filename, "w+b");
    if (filePointer== NULL) {
        return ERR_IO; // Return an error code
    }

    imgfs_file->file= filePointer;
    size_t bytes_w = fwrite(&imgfs_file->header, sizeof(struct imgfs_header),  1,filePointer);
    if (bytes_w != 1){
        return ERR_IO;
    }

    uint32_t num_files = imgfs_file->header.max_files;
    imgfs_file->metadata = calloc(num_files,sizeof(struct img_metadata));
    if (imgfs_file->metadata == NULL){
        return ERR_OUT_OF_MEMORY;
    }
    bytes_w += fwrite(imgfs_file->metadata, sizeof(struct img_metadata),num_files,filePointer);
    if (bytes_w != num_files+1){
        return ERR_IO;
    }
    printf("%zu item(s) written", bytes_w);
    return ERR_NONE;
}
