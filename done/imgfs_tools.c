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
            struct imgfs_file* imgfs_file)
{
    //Checking the validity of the pointers
    M_REQUIRE_NON_NULL(imgfs_filename);
    M_REQUIRE_NON_NULL(open_mode);
    M_REQUIRE_NON_NULL(imgfs_file);

    // Open file in specified open_mode
    imgfs_file->file=fopen(imgfs_filename,open_mode);
    if (imgfs_file->file ==NULL) return ERR_IO;

    // Read the contents of the header
    if (fread(&(imgfs_file->header), sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
        do_close(imgfs_file);
        return ERR_IO;
    }
    // Allocate memory in metadata array
    imgfs_file->metadata = calloc(imgfs_file->header.max_files, sizeof(struct img_metadata));
    if (imgfs_file->metadata == NULL) {
        do_close(imgfs_file);
        return ERR_OUT_OF_MEMORY;
    }

    // Read the contents of the metadata
    int i = 0;
    while (i < (int)(imgfs_file->header.max_files)) {
        size_t read = fread(&imgfs_file->metadata[i], sizeof(struct img_metadata), 1, imgfs_file->file);
        if (read != 1) {
            do_close(imgfs_file);
            return ERR_IO;
        }
        i++;
    }

    // All good
    return ERR_NONE;

}

/**
 * @brief Do some clean-up for imgFS file handling.
 *
 * @param imgfs_file Structure for header, metadata and file pointer to be freed/closed.
 */
void do_close(struct imgfs_file* imgfs_file)
{
    if (imgfs_file!=NULL ) {
        // Close the file and make the file pointer point to NULL
        if (imgfs_file->file !=NULL) {
            fclose(imgfs_file->file);
            imgfs_file->file= NULL;
        }
        // Free space allocated for metadata and make the metadata pointer point to NULL
        if (imgfs_file->metadata !=NULL) {
            free(imgfs_file->metadata);
            imgfs_file->metadata= NULL;
        }

    }

}

int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size) {
    M_REQUIRE_NON_NULL(path);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);

    // Open the file
    FILE *file = fopen(path, "rb");
    if (!file) {
        return ERR_IO;
    }

    // Get the size of the file
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size == -1) {
        fclose(file);
        return ERR_IO;
    }
    rewind(file);

    // Allocate memory for the file content
    *image_buffer = (char *)malloc(size);
    if (!*image_buffer) {
        fclose(file);
        return ERR_OUT_OF_MEMORY;
    }

    // Read the file into the buffer
    size_t read_size = fread(*image_buffer, 1, size, file);
    if (read_size != (size_t)size) {
        free(*image_buffer);
        *image_buffer = NULL;
        fclose(file);
        return ERR_IO;
    }

    // Set the image size to the size of the file
    *image_size = (uint32_t)size;

    // Close the file
    fclose(file);
    return ERR_NONE;
}

int resolution_atoi (const char* str)
{
    if (str == NULL) return -1;

    if (!strcmp(str, "thumb") || !strcmp(str, "thumbnail")) {
        return THUMB_RES;
    } else if (!strcmp(str, "small")) {
        return SMALL_RES;
    } else if (!strcmp(str, "orig")  || !strcmp(str, "original")) {
        return ORIG_RES;
    }
    return -1;
}



