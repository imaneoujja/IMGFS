#include "imgfs.h"  // for struct imgfs_file
#include <string.h> // for strncmp
#include "error.h" // for error codes
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
              const char* img_id, struct imgfs_file* imgfs_file)
{
    // Checking the validity of the parameters
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);


    if(image_size <= 0) return ERR_INVALID_ARGUMENT;


    // Check if the image file system is full
    uint32_t max_files=imgfs_file->header.max_files;
    if(imgfs_file->header.nb_files >= max_files) return ERR_IMGFS_FULL;



    int index = -1;
    for (int i = 0; i < max_files; ++i) {
        if (imgfs_file->metadata[i].is_valid == EMPTY) {
            index = i;
            break;
        }
    }
    if (index == -1) return ERR_IMGFS_FULL;

    struct img_metadata previous = imgfs_file->metadata[index];
    // Compute the SHA-256 hash of the image
    SHA256( (const unsigned char*) image_buffer, image_size, imgfs_file->metadata[index].SHA);
    strcpy(imgfs_file->metadata[index].img_id, img_id);

    imgfs_file->metadata[index].size[ORIG_RES] = (uint32_t) image_size;

    uint32_t width ;
    uint32_t height ;

    // Get the resolution of the image
    int err = get_resolution(&height, &width, image_buffer, image_size);
    if (err != ERR_NONE) return err;


    //Initializing variables
    imgfs_file->metadata[index].unused_16 = EMPTY;
    imgfs_file->metadata[index].is_valid = NON_EMPTY;

    imgfs_file->metadata[index].orig_res[0] = width;
    imgfs_file->metadata[index].orig_res[1] = height;

    // grader: (malus) EMPTY is meant to be used only for is_valid (-1)
    imgfs_file->metadata[index].size[THUMB_RES] = EMPTY;
    imgfs_file->metadata[index].size[SMALL_RES] = EMPTY;

    imgfs_file->metadata[index].offset[THUMB_RES] = EMPTY;
    imgfs_file->metadata[index].offset[SMALL_RES] = EMPTY;


    imgfs_file->header.nb_files++;
    imgfs_file->header.version++;

    // Perform deduplication
    int dedup = do_name_and_content_dedup(imgfs_file, index);
    if (dedup != ERR_NONE) {
        memcpy(&imgfs_file->metadata[index], &previous, sizeof(struct img_metadata));
        return dedup;
    }

    // Write the image to the file if not already present
    if (imgfs_file->metadata[index].offset[ORIG_RES] == EMPTY) {
	// grader: EMPTY is not meant to be used as an argument to fseek
        fseek(imgfs_file->file, EMPTY, SEEK_END);
        uint64_t pos = ftell(imgfs_file->file);
        size_t write = fwrite(image_buffer, NON_EMPTY, image_size, imgfs_file->file);
        if (write != image_size) return ERR_IO;

        imgfs_file->metadata[index].offset[ORIG_RES] = pos;
    }
    // Update the header
    if(fseek(imgfs_file->file, EMPTY, SEEK_SET) )   return ERR_IO;
    if ((fwrite(&imgfs_file->header, sizeof(char), sizeof(struct imgfs_header), imgfs_file->file)) != sizeof(struct imgfs_header)) return ERR_IO;

    //Update the metadta
    if(fseek(imgfs_file->file, sizeof(struct imgfs_header) +  sizeof(struct img_metadata)*index, SEEK_SET) ) return ERR_IO;
    if ((fwrite(&imgfs_file->metadata[index], sizeof(char), sizeof(struct img_metadata), imgfs_file->file)) != sizeof(struct img_metadata)) return ERR_IO;

    return ERR_NONE;
}
