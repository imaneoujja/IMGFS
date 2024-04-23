
#include "imgfs.h"
#include "error.h"
#include <string.h>

int do_delete(const char* img_id, struct imgfs_file* imgfs_file) {
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);

    if (imgfs_file->file == NULL) {
        return ERR_IO; 
    }

    for (int i = 0; i < imgfs_file->header.max_files; ++i) {
        if (strcmp(imgfs_file->metadata[i].img_id, img_id) == 0) {
            imgfs_file->metadata[i].is_valid = EMPTY;
            imgfs_file->header.version++; 
            imgfs_file->header.nb_files--; 

        
            fseek(imgfs_file->file, 0, SEEK_SET); 
            fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file); 
            fwrite(imgfs_file->metadata, sizeof(struct img_metadata), imgfs_file->header.max_files, imgfs_file->file); 
            fflush(imgfs_file->file); 

            return ERR_NONE; 
        }
    }

    return ERR_IMAGE_NOT_FOUND;

}