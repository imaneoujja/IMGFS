/* ** NOTE: undocumented in Doxygen
 * @file imgfs_tools.c
 * @brief implementation of several tool functions for imgFS
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "util.h"

#include <inttypes.h>      // for PRIxN macros
#include <openssl/sha.h>   // for SHA256_DIGEST_LENGTH
#include <stdint.h>        // for uint8_t
#include <stdio.h>         // for sprintf
#include <stdlib.h>        // for calloc
#include <string.h>        // for strcmp

/*******************************************************************
 * Human-readable SHA
 */
static void sha_to_string(const unsigned char* SHA,
                          char* sha_string)
{
    if (SHA == NULL) return;

    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(sha_string + (2 * i), "%02x", SHA[i]);
    }

    sha_string[2 * SHA256_DIGEST_LENGTH] = '\0';
}

/*******************************************************************
 * imgFS header display.
 */
void print_header(const struct imgfs_header* header)
{
    printf("*****************************************\n\
********** IMGFS HEADER START ***********\n");
    printf("TYPE: " STR_LENGTH_FMT(MAX_IMGFS_NAME) "\
\nVERSION: %" PRIu32 "\n\
IMAGE COUNT: %" PRIu32 "\t\tMAX IMAGES: %" PRIu32 "\n\
THUMBNAIL: %" PRIu16 " x %" PRIu16 "\tSMALL: %" PRIu16 " x %" PRIu16 "\n",
           header->name, header->version, header->nb_files, header->max_files, header->resized_res[THUMB_RES * 2],
           header->resized_res[THUMB_RES * 2 + 1], header->resized_res[SMALL_RES * 2],
           header->resized_res[SMALL_RES * 2 + 1]);
    printf("*********** IMGFS HEADER END ************\n\
*****************************************\n");
}

/*******************************************************************
 * Metadata display.
 */
void print_metadata (const struct img_metadata* metadata)
{
    char sha_printable[2 * SHA256_DIGEST_LENGTH + 1];
    sha_to_string(metadata->SHA, sha_printable);

    printf("IMAGE ID: %s\nSHA: %s\nVALID: %" PRIu16 "\nUNUSED: %" PRIu16 "\n\
OFFSET ORIG. : %" PRIu64 "\t\tSIZE ORIG. : %" PRIu32 "\n\
OFFSET THUMB.: %" PRIu64 "\t\tSIZE THUMB.: %" PRIu32 "\n\
OFFSET SMALL : %" PRIu64 "\t\tSIZE SMALL : %" PRIu32 "\n\
ORIGINAL: %" PRIu32 " x %" PRIu32 "\n",
           metadata->img_id, sha_printable, metadata->is_valid, metadata->unused_16, metadata->offset[ORIG_RES],
           metadata->size[ORIG_RES], metadata->offset[THUMB_RES], metadata->size[THUMB_RES],
           metadata->offset[SMALL_RES], metadata->size[SMALL_RES], metadata->orig_res[0], metadata->orig_res[1]);
    printf("*****************************************\n");
}

/**
 * @brief Open imgFS file, read the header and all the metadata.
 *
 * @param imgfs_filename Path to the imgFS file
 * @param open_mode Mode for fopen(), eg.: "rb", "rb+", etc.
 * @param imgfs_file Structure for header, metadata and file pointer.
 */
int do_open(const char* imgfs_filename,
            const char* open_mode,
            struct imgfs_file* imgfs_file){
    M_REQUIRE_NON_NULL(imgfs_filename);
    M_REQUIRE_NON_NULL(open_mode);

    FILE *filePointer;

    if (imgfs_file == NULL) {
        return ERR_INVALID_ARGUMENT; // Return an error code
    }
    filePointer = fopen(imgfs_filename, open_mode);
    if (filePointer== NULL) {
        return ERR_IO; // Return an error code
    }

    imgfs_file->file= filePointer;
    size_t bytes_read = fread(imgfs_file->header.name, sizeof(char),MAX_IMGFS_NAME + 1,filePointer);
    if (bytes_read != MAX_IMGFS_NAME+1){
        return ERR_IO;
    }
    bytes_read = fread(&imgfs_file->header.version, sizeof(uint32_t),1,filePointer);
    if (bytes_read != 1){
        return ERR_IO;
    }
    bytes_read = fread(&imgfs_file->header.nb_files,sizeof(uint32_t),1,filePointer);
    if (bytes_read != 1){
        return ERR_IO;
    }
    bytes_read = fread(&imgfs_file->header.max_files,sizeof(uint32_t ),1,filePointer);
    if (bytes_read != 1){
        return ERR_IO;
    }
    bytes_read = fread(imgfs_file->header.resized_res,sizeof(uint16_t),(NB_RES - 1) * 2,filePointer);
    if (bytes_read != (NB_RES - 1) * 2){
        return ERR_IO;
    }
    bytes_read = fread(&imgfs_file->header.unused_32,sizeof(uint32_t),1,filePointer);
    if (bytes_read != 1){
        return ERR_IO;
    }
    bytes_read = fread(&imgfs_file->header.unused_64,sizeof(uint64_t),1,filePointer);
    if (bytes_read != 1){
        return ERR_IO;
    }

    int num_files = imgfs_file->header.nb_files;
    imgfs_file->metadata = calloc(num_files,sizeof(struct img_metadata));
    if (imgfs_file->metadata == NULL){
        return ERR_OUT_OF_MEMORY;
    }
    for (int i = 0;i<num_files;i++){
        bytes_read = fread(imgfs_file->metadata[i].img_id, sizeof(char),MAX_IMG_ID + 1,filePointer);
        if (bytes_read != MAX_IMG_ID+1){
            return ERR_IO;
        }
        bytes_read = fread(imgfs_file->metadata[i].SHA, sizeof(unsigned char),SHA256_DIGEST_LENGTH,filePointer);
        if (bytes_read != SHA256_DIGEST_LENGTH){
            return ERR_IO;
        }
        bytes_read = fread(imgfs_file->metadata[i].orig_res, sizeof(uint32_t),2,filePointer);
        if (bytes_read != 2){
            return ERR_IO;
        }
        bytes_read = fread(imgfs_file->metadata[i].size, sizeof(uint32_t),NB_RES,filePointer);
        if (bytes_read != NB_RES){
            return ERR_IO;
        }
        bytes_read = fread(imgfs_file->metadata[i].offset, sizeof(uint64_t),NB_RES,filePointer);
        if (bytes_read != NB_RES){
            return ERR_IO;
        }
        bytes_read = fread(&imgfs_file->metadata[i].is_valid,sizeof(uint16_t ),1,filePointer);
        if (bytes_read != 1){
            return ERR_IO;
        }
        bytes_read = fread(&imgfs_file->metadata[i].unused_16,sizeof(uint16_t),1,filePointer);
        if (bytes_read != 1){
            return ERR_IO;
        }
    }

    return ERR_NONE;
}

/**
 * @brief Do some clean-up for imgFS file handling.
 *
 * @param imgfs_file Structure for header, metadata and file pointer to be freed/closed.
 */
void do_close(struct imgfs_file* imgfs_file){
    if (imgfs_file!=NULL ){
        if (imgfs_file->file !=NULL){
            fclose(imgfs_file->file);
        }

        free(imgfs_file->metadata);
        imgfs_file->metadata= NULL;
    }

}

