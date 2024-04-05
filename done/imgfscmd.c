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
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>

typedef int (*CommandFunction)(int argc, char* argv[]);

struct command_mapping {
    const char* name;
    CommandFunction function;
};

struct command_mapping commands[] = {
    {"list", do_list_cmd},
    {"create", do_create_cmd},
    {"help", help},
    {"delete", do_delete_cmd}
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/*******************************************************************************
 * MAIN
 */
int main(int argc, char* argv[])
{
    int ret = 0;

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        int i;
        /* **********************************************************************
         * TODO WEEK 07: THIS PART SHALL BE EXTENDED.
         * **********************************************************************
         */
        for (i = 0; i < NUM_COMMANDS; i++) {
            if (strcmp(argv[0], commands[i].name) == 0) {
                i = NUM_COMMANDS;
                ret = commands[i].function(argc, argv);
            }
        }

        // If command not found, call help() and return ERR_INVALID_COMMAND
        if (i == NUM_COMMANDS) {
            help(argc,argv);
            ret = ERR_INVALID_COMMAND;
        }

        argc--; argv++; // skips command call name
    }

    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc, argv);
    }

    return ret;
}
