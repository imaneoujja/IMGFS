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


/*******************************************************************************
 * MAIN
 */
int main(int argc, char* argv[])
{
    M_REQUIRE_NON_NULL(argv);
    int ret = 0;
    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        argc -- ; argv++;
        int foundCommand=0;
        int i =0;
        for(int i = 0; i<4 ; i++){
            if (strcmp(argv[0], commands[i].name)==0){
                ret = commands[i].function(argc-1, argv+1);
                foundCommand=1;
                break;
            }

        }

        if (!foundCommand) {
            help(argc,argv);
            ret = ERR_INVALID_COMMAND;
        }
        
    }
    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc, argv);
    }
    
    return ret;
}