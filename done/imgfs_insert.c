#include "imgfs.h"  // for struct imgfs_file
#include <string.h> // for strncmp
#include "image_dedup.h" // for do_name_and_content_dedup()
/**
 * @brief Insert image in the imgFS file
 *
 * @param buffer Pointer to the raw image content
 * @param size Image size
 * @param img_id Image ID
 * @return Some error code. 0 if no error.
 */
int do_insert(const char* image_buffer, size_t image_size,
              const char* img_id, struct imgfs_file* imgfs_file){
    // Checking the validity of the pointers
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    if (imgfs_file.imgfs_header.nb_files >= imgfs_file.imgfs_header.max_files){
        return ERR_IMGFS_FULL;
    }
    for (int i=0;i<imgfs_file.header.nb_files;i++){
        //Check if image is empty
        if (!imgfs_file->metadata[i].is_valid){
            //Initialize the metadata
            SHA256(image_buffer, image_size,imgfs_file->metadata[i].SHA );
            strncpy(imgfs_file->metadata[i].img_id, img_id,MAX_IMG_ID + 1);
            imgfs_file->metadata[i].size[ORIG_RES] = (uint32_t)image_size;
            uint32_t *height;
            uint32_t *width;
            //Get width and height
            int error = get_resolution(height, width,image_buffer, image_size);
            if (error != ERR_NONE){
                return error;
            }
            imgfs_file->metadata[i].orig_res = {width*,height*};
            imgfs_file->metadata[i].is_valid = NON_EMPTY;
            //Check whether image is already in database or img_id belongs to another image
            error = do_name_and_content_dedup(imgfs_file,i);
            if (error != ERR_NONE){
                return error;
            }
            //If image is not already in database, write it to file
            if (imgfs_file->metadata[i].offset[ORIG_RES] == 0){
                size_t bytes_w = fwrite(image_buffer, image_size,1,imgfs_file->file);
                if (bytes_w != 1){
                    return ERR_IO;
                }
            }
            //Update all the necessary image database header fields and write header to file
            imgfs_file.header.version += 1;
            imgfs_file.header.nb_files += 1;
            bytes_w = fwrite(&imgfs_file.header,sizeof(struct imgfs_header),1,imgfs_file->file);
            if (bytes_w != 1){
                return ERR_IO;
            }
            bytes_w = fwrite(&imgfs_file->metadata[i], sizeof(struct img_metadata),1,imgfs_file->file);
            if (bytes_w != 1){
                return ERR_IO;
            }
            return ERR_NONE;
        }
    }
}