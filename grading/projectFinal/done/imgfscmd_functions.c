/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Marta Adarve de Leon & Imane Oujja
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
    printf("          %s <MAX_FILES>: maximum number of files.\n", MAX_FILES_OPTION);
    printf("                                  default value is %d\n", default_max_files);
    printf("                                  maximum value is %u\n", UINT32_MAX);
    printf("          %s <X_RES> <Y_RES>: resolution for thumbnail images.\n", THUMB_RES_OPTION);
    printf("                                  default value is %dx%d\n", default_thumb_res, default_thumb_res);
    printf("                                  maximum value is %dx%d\n", MAX_THUMB_RES, MAX_THUMB_RES);
    printf("          %s <X_RES> <Y_RES>: resolution for small images.\n", SMALL_RES_OPTION);
    printf("                                  default value is %dx%d\n", default_small_res, default_small_res);
    printf("                                  maximum value is %dx%d\n", MAX_SMALL_RES, MAX_SMALL_RES);
    printf("  read <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n");
    printf("      read an image from the imgFS and save it to a file.\n");
    printf("      default resolution is \"original\".\n");
    printf("  insert <imgFS_filename> <imgID> <filename>: insert a new image in the imgFS.\n");
    printf("  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n");
    return ERR_NONE;
}

/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 1) {
        return ERR_INVALID_COMMAND;
    }
    const char* imgfs_filename = argv[0];
    struct imgfs_file imgfs_file;
    // Open imgFS file, read the header and all the metadata.
    int err = do_open(imgfs_filename, "rb", &imgfs_file);
    if (err != ERR_NONE) {
        return err;
    }

    enum do_list_mode output_mode = STDOUT;
    char** json = NULL;
    // Display (on stdout) imgFS metadata then close the gile
    int error=do_list(&imgfs_file, output_mode, json);
    do_close(&imgfs_file);
    return error;

}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */
int do_create_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (!argc) return ERR_NOT_ENOUGH_ARGUMENTS;


    struct imgfs_file file;
    memset(&file,0,sizeof(struct imgfs_file));
    int resized_res_i = 0;
    int max_size;
    // Initialise all header fields to default
    file.header.max_files = default_max_files;
    file.header.resized_res[2 * THUMB_RES] = default_thumb_res;
    file.header.resized_res[2 * THUMB_RES + 1] = default_thumb_res;
    file.header.resized_res[2 * SMALL_RES] = default_small_res;
    file.header.resized_res[2 * SMALL_RES + 1] = default_small_res;
    // While there are arguments to be read, keep (re)defining fields of the header
    for (int i = 1; i < argc; ++i) {
        // Check whether argument matches -max_files
        if (strcmp(argv[i], MAX_FILES_OPTION) == 0) {
            // Need at least one argument after -max_files to specify number of max files
            if (i + 1 >= argc) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }

            file.header.max_files = atouint32(argv[i + 1]);

            if (file.header.max_files == 0 ) {
                return ERR_MAX_FILES;
            }
            // Advance index so it can read string specifying next field to be set
            i++;
        } // Check whether argument matches -thumb_res or -small_res
        else if (strcmp(argv[i], THUMB_RES_OPTION) == 0 || strcmp(argv[i], SMALL_RES_OPTION) == 0) {
            // Need at least two more arguments to specify width and height of particular resolution
            if (i + 2 >= argc) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }

            if (strcmp(argv[i], THUMB_RES_OPTION) == 0) {
                max_size = MAX_THUMB_RES; // Set max size for width and height in the case of the thumb resolution
                resized_res_i = THUMB_RES; // Set index that is to be modified in the resolutions array
            } else {
                max_size = MAX_SMALL_RES; // Set max size for width and height in the case of the small resolution
                resized_res_i = SMALL_RES; // Set index that is to be modified in the resolutions array
            }
            // Modify width and height in resolutions array of the header
            const char * width = argv[i + 1];
            const char * height = argv[i + 2];
            file.header.resized_res[2*resized_res_i] = atouint16(width);
            file.header.resized_res[2*resized_res_i + 1] = atouint16(height);
            // Check resolution's width and height are non-negative and smaller than max size
            if (file.header.resized_res[resized_res_i]  <= 0 || file.header.resized_res[resized_res_i+1] <= 0 || file.header.resized_res[resized_res_i] > max_size || file.header.resized_res[resized_res_i+1] > max_size) {
                return ERR_RESOLUTIONS;
            }
            // Advance index of next argv to be read, so it can read string specifying next field to be set
            i += 2;
        } else {
            return ERR_INVALID_ARGUMENT;
        }
    }
    //  Creates the imgFS named as filename stored in argv[0]. Writes the header and the
    //  preallocated empty metadata array to imgFS file.
    int err = do_create(argv[0],&file);
    do_close(&file);
    return err;
}

/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char** argv)
{
    // Check the validity of the pointer
    M_REQUIRE_NON_NULL(argv);

    // Return error code if not enough arguments
    if (argc < 2) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    // Extracting the filename and the ID
    const char* imgfs_filename = argv[0];
    const char* img_id = argv[1];

    // Return error when img_id is empty or exceeds maximum length
    if (img_id == NULL || strlen(img_id) > MAX_IMG_ID) {
        return ERR_INVALID_IMGID;
    }

    struct imgfs_file imgfs_file;
    // Open imgFS file for reading and writing, return error if fail
    int ret = do_open(imgfs_filename, "rb+", &imgfs_file);
    if (ret != ERR_NONE) {
        return ret;
    }

    // Delete the image from the imgFS file, return error if fail
    ret = do_delete(img_id, &imgfs_file);
    if (ret != ERR_NONE) {
        do_close(&imgfs_file);
        return ret;
    }

    //Close ImgFS file
    do_close(&imgfs_file);

    //All good
    return ERR_NONE;
}

/**********************************************************************
 *  Create the name of the file to use to save the read image
 */

static void create_name(const char* img_id, int resolution, char** new_name)
{
    if (resolution < 0 || resolution >= NB_RES  || !img_id  || !new_name ) {
        return;
    }
    const char* resolutions[] = {"_thumb", "_small", "_orig"};
    const char* extension = ".jpg";
    // Calculate the length of the new name
    size_t name_length = strlen(img_id) + strlen(resolutions[resolution]) + strlen(extension) + 1;

    *new_name = (char*) malloc(name_length);
    if (*new_name == NULL) {
        return;
    }
    // Construct the new name
    snprintf(*new_name, name_length, "%s%s%s", img_id, resolutions[resolution], extension);
}

/**********************************************************************
 * Write content of buffer to file
 */
static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size)
{
    // Check validty of pointers
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(image_buffer);
    // Open file to write in binary mode
    FILE* file = fopen(filename,"wb");
    // Write content of buffer to file
    size_t bytes_w = fwrite(image_buffer,image_size,1,file);
    fclose(file);
    if (bytes_w != 1) return ERR_IO;

    return ERR_NONE;
}

/********************************************************************
 * Inserts an image into the imgFS.
 *******************************************************************/
int do_insert_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    // Open the ImgFS file in read-write mode
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size;

    // Reads image from the disk.
    error = read_disk_image (argv[2], &image_buffer, &image_size);
    if (error != ERR_NONE) {
        do_close(&myfile);
        return error;
    }
    // Insert the image into the ImgFS
    error = do_insert(image_buffer, image_size, argv[1], &myfile);
    free(image_buffer);
    do_close(&myfile);
    return error;
}


/********************************************************************
 * Reads an image from the imgFS.
 *******************************************************************/
int do_read_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 2 && argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char * const img_id = argv[1];

    const int resolution = (argc == 3) ? resolution_atoi(argv[2]) : ORIG_RES;
    if (resolution == -1) return ERR_RESOLUTIONS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    // Open the image from the ImgFS
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size = 0;
    // Read the image from the ImgFS
    error = do_read(img_id, resolution, &image_buffer, &image_size, &myfile);
    do_close(&myfile);
    if (error != ERR_NONE) return error;

    // Extracting to a separate image file.
    char* tmp_name = NULL;
    create_name(img_id, resolution, &tmp_name);
    if (tmp_name == NULL) return ERR_OUT_OF_MEMORY;
    error = write_disk_image(tmp_name, image_buffer, image_size);

    free(tmp_name);
    free(image_buffer);

    return error;
}

/********************************************************************
 * Reads an image from disk into a buffer.
 *******************************************************************/

int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size)
{
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
