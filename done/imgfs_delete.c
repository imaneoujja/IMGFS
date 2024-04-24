
#include "imgfs.h"
#include "error.h"
#include <string.h>

int do_delete(const char* img_id, struct imgfs_file* imgfs_file) {
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(img_id);


    int found = -1;

    for (uint32_t i = 0; i < imgfs_file->header.max_files; i++) {
        if (imgfs_file->metadata[i].is_valid != EMPTY && strcmp(imgfs_file->metadata[i].img_id, img_id) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        return ERR_IMAGE_NOT_FOUND;
    }

    imgfs_file->metadata[found].is_valid = EMPTY;
    
    fseek(imgfs_file->file, sizeof(struct imgfs_header) + found * sizeof(struct img_metadata), SEEK_SET);
    if (fwrite(&imgfs_file->metadata[found], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }
    

    imgfs_file->header.nb_files--;
    imgfs_file->header.version++;

    rewind(imgfs_file->file);
    if (fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    return ERR_NONE;


}