
/*
 * @file imgfs_server.c
 * @brief ImgFS server part, main
 *
 * @author Marta Adarve de Leon & Imane Oujja
 */

#include "util.h"
#include "imgfs.h"
#include "socket_layer.h"
#include "http_net.h"
#include "imgfs_server_service.h"
#include <vips/vips.h>


#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h> // abort()

/************************
Signal handler function to handle termination signals.
 *************************/

static void signal_handler(int sig_num)
{
    server_shutdown();
    exit(EXIT_SUCCESS);  // Ensure clean exit
}

/************************
 Sets up the signal handler for termination signals.
 *************************/

static void set_signal_handler(void)
{
    struct sigaction action;
    if (sigemptyset(&action.sa_mask) == -1) {
        perror("sigemptyset() in set_signal_handler()");
        abort();
    }
    action.sa_handler = signal_handler;
    action.sa_flags   = 0;
    if ((sigaction(SIGINT,  &action, NULL) < 0) ||
        (sigaction(SIGTERM,  &action, NULL) < 0)) {
        perror("sigaction() in set_signal_handler()");
        abort();
    }
}



/************************
Main function for the ImgFS server.
 *************************/
int main (int argc, char *argv[])
{

    int err = server_startup(argc, argv);
    if (err != ERR_NONE) {
        return err;
    }
    if (VIPS_INIT(argv[0])) {
        return ERR_IMGLIB;
    }
    set_signal_handler();

    while ((err = http_receive()) == ERR_NONE);

    fprintf(stderr, "http_receive() failed\n");
    fprintf(stderr, "%s\n", ERR_MSG(err));
    vips_shutdown();

    return ERR_NONE;
}
