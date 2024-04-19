
#include "imgfs.h"
#include "error.h"
#include <string.h>

int do_delete(const char* img_id, struct imgfs_file* imgfs_file) {
    M_REQUIRE_NON_NULL(img_id);
    if (strlen(img_id)==0 ||  strlen(img_id) > MAX_IMG_ID) {
        return ERR_INVALID_IMGID;
    }

    int index = -1;
    for (int i = 0; i < imgfs_file->header.max_files; ++i) {
        if (strcmp(imgfs_file->metadata[i].img_id, img_id) == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return ERR_IMAGE_NOT_FOUND;
    }

    imgfs_file->metadata[index].is_valid = EMPTY;
    imgfs_file->header.version++;
    imgfs_file->header.nb_files--;

    fseek(imgfs_file->file, 0, SEEK_SET);
    fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file);


    return ERR_NONE;
}