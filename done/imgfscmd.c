/**
 * @file imgfscmd.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * Image Filesystem Command Line Tool
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"

#include <string.h>
#include <vips/vips.h>


// Function pointer for command functions
typedef int (*CommandFunction)(int argc, char* argv[]);

// Structure mapping command names to the corresponding functions
struct command_mapping {
    const char* name;
    CommandFunction function;
};

// Array of command mappings
struct command_mapping commands[] = {
    {"list", do_list_cmd},
    {"create", do_create_cmd},
    {"help", help},
    {"delete", do_delete_cmd}
};

// Constant of the umber of commands in the commands array
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))


/*******************************************************************************
 * MAIN
 */
int main(int argc, char* argv[])
{
    // Check the validity of the pointer
    M_REQUIRE_NON_NULL(argv);

    // Initialize VIPS library
    if (VIPS_INIT(argv[0])) {  
        return ERR_IMGLIB;
    }
    int ret = 0;
    // Not enough arguments 
    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        // Skip program name
        argc -- ; argv++;
        int definedCommand=0;
        // Iterate through the commands array
        for(unsigned long i = 0; i<NUM_COMMANDS ; i++) {
            if (strcmp(argv[0], commands[i].name)==0) {
                // Skip command name
                argc -- ; argv++;
                // Call the corresponding function if the command is found
                ret = commands[i].function(argc, argv);
                definedCommand=1;
                break;
            }

        }

        // Command is not defined
        if (!definedCommand) {
            help(argc,argv);
            ret = ERR_INVALID_COMMAND;
        }

    }
    // Display eroor message and call help function
    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc, argv);
    }
    vips_shutdown();

    // Return the error 
    return ret;
}
