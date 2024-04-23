/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused


#include "string.h"

#define MAX_FILES_OPTION "-max_files"
#define THUMB_RES_OPTION "-thumb_res"
#define SMALL_RES_OPTION "-small_res"

// default values
static const uint32_t default_max_files = 128;
static const uint16_t default_thumb_res =  64;
static const uint16_t default_small_res = 256;

// max values
static const uint16_t MAX_THUMB_RES = 128;
static const uint16_t MAX_SMALL_RES = 512;

/**********************************************************************
 * Displays some explanations.
 ********************************************************************** */
int help(int useless _unused, char** useless_too _unused)
{
    printf("imgfscmd [COMMAND] [ARGUMENTS]\n");
    printf("  help: displays this help.\n");
    printf("  list <imgFS_filename>: list imgFS content.\n");
    printf("  create <imgFS_filename> [options]: create a new imgFS.\n");
    printf("      options are:\n");
    printf("          -max_files <MAX_FILES>: maximum number of files.\n");
    printf("                                  default value is 128\n");
    printf("                                  maximum value is 4294967295\n");
    printf("          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n");
    printf("                                  default value is 64x64\n");
    printf("                                  maximum value is 128x128\n");
    printf("          -small_res <X_RES> <Y_RES>: resolution for small images.\n");
    printf("                                  default value is 256x256\n");
    printf("                                  maximum value is 512x512\n");
    printf("  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n");
    return ERR_NONE;
}

/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);
    
    if (argc != 2) {
        if (argc ==1){
            return ERR_IO;}
        return ERR_INVALID_COMMAND;
    }
    const char* imgfs_filename = argv[1];
    struct imgfs_file imgfs_file;
    int err = do_open(imgfs_filename, "rb", &imgfs_file);
    if (err != ERR_NONE) {
        return err;
    }
    enum do_list_mode output_mode = STDOUT;
    char** json = NULL;
    int error=do_list(&imgfs_file, output_mode, json);   
    if(error!=ERR_NONE){
        do_close(&imgfs_file);
        return error;
    }
    do_close(&imgfs_file);

    return ERR_NONE;

}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */
int do_create_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc ==0){
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    struct imgfs_file file;
    int resized_res_i = 0;
    int max_size;
    file.header.max_files = default_max_files;
    file.header.resized_res[2 * THUMB_RES] = default_thumb_res;
    file.header.resized_res[2 * THUMB_RES + 1] = default_thumb_res;
    file.header.resized_res[2 * SMALL_RES] = default_small_res;
    file.header.resized_res[2 * SMALL_RES + 1] = default_small_res;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], MAX_FILES_OPTION) == 0) {
            if (i + 1 >= argc) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }

            file.header.max_files = atouint32(argv[i + 1]);

            if (file.header.max_files == 0 || file.header.max_files > default_max_files) {
                return ERR_MAX_FILES;
            }

            i++;
        } else if (strcmp(argv[i], THUMB_RES_OPTION) == 0 || strcmp(argv[i], SMALL_RES_OPTION) == 0) {
            if (i + 2 >= argc) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
            if (strcmp(argv[i], THUMB_RES_OPTION) == 0) {
                max_size = MAX_THUMB_RES;
                resized_res_i = THUMB_RES;
            }
            else{
                max_size = MAX_SMALL_RES;
                resized_res_i = SMALL_RES;
            }
            file.header.resized_res[2*resized_res_i] = atouint16(argv[i + 1]);
            file.header.resized_res[2*resized_res_i + 1] = atouint16(argv[i + 2]);
            if (file.header.resized_res[resized_res_i]  == 0 || file.header.resized_res[resized_res_i+1] == 0 || file.header.resized_res[resized_res_i] > max_size || file.header.resized_res[resized_res_i+1] > max_size) {
                return ERR_RESOLUTIONS;
            }
            i += 2;
        } else {
            return ERR_INVALID_ARGUMENT;
        }
    }
    int err = do_create(argv[0],&file);
    do_close(&file);
    return err;
}

/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char** argv)
{
    if (argc != 3) {
        return ERR_INVALID_ARGUMENT;
    }

    const char* imgfs_filename = argv[1];
    const char* img_id = argv[2];

    if (img_id == NULL || strlen(img_id) == 0 || strlen(img_id) > MAX_IMG_ID) {
        return ERR_INVALID_IMGID;
    }

    struct imgfs_file imgfs_file;
    int ret = do_open(imgfs_filename, "rb+", &imgfs_file);
    if (ret != ERR_NONE) {
        return ret;
    }

    ret = do_delete(img_id, &imgfs_file);
    if (ret != ERR_NONE) {
        do_close(&imgfs_file);
        return ret;
    }

    do_close(&imgfs_file);

    return ERR_NONE;
}

