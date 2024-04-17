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
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE.
     * **********************************************************************
     */

    TO_BE_IMPLEMENTED();
    return NOT_IMPLEMENTED;
}

/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char** argv)
{

    M_REQUIRE_NON_NULL(argv);
    M_REQUIRE_NON_NULL(argv[0]);
    if (argc !=1 ){
        return ERR_INVALID_COMMAND;
    }
    struct imgfs_file file;
    file.file = NULL;
    file.metadata = NULL;
    int err = do_open(argv[0],
                      "rb",
                      &file);
    if (err == ERR_NONE){
        enum do_list_mode output_mode = STDOUT;
        err = do_list(&file,output_mode, NULL);
    }

    do_close(&file);
    return err;
}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */
int do_create_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);
    M_REQUIRE_NON_NULL(argv[1]);
    puts("Create");
    if (argc < 2) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    struct imgfs_file file;
    int resized_res_i = 0;
    int max_size;
    for (int i = 2; i < argc; ++i) {
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
            file.header.resized_res[resized_res_i] = atouint16(argv[i + 1]);
            file.header.resized_res[resized_res_i + 1] = atouint16(argv[i + 2]);
            if (strcmp(argv[i], THUMB_RES_OPTION) == 0) {
                max_size = MAX_THUMB_RES;
            }
            else{
                max_size = MAX_SMALL_RES;
            }
            if (file.header.resized_res[resized_res_i]  == 0 || file.header.resized_res[resized_res_i+1] == 0 || file.header.resized_res[resized_res_i] > max_size || file.header.resized_res[resized_res_i+1] > max_size) {
                return ERR_RESOLUTIONS;
            }
            resized_res_i += 2;
            i += 2;
        } else {
            return ERR_INVALID_ARGUMENT;
        }
    }
    int err = do_create(argv[1],&file);
    return err;
}

/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char** argv)
{
    /* **********************************************************************
     * TODO WEEK 08: WRITE YOUR CODE HERE (and change the return if needed).
     * **********************************************************************
     */

    TO_BE_IMPLEMENTED();
    return NOT_IMPLEMENTED;
}
