/** 
 * Module to handle events related to the outptu
 */

#ifndef OUTPUT_H
#define OUTPUT_H

#include "server.h"
#include <wayland-server-core.h>
#include <wayland-util.h>

typedef struct {
    vdbwm_server* _server;
    struct wl_list _outputs;
    vlistener _new_output_listener;
} output_manager;

/**
 * Initializes the output module  and registers it's callbacks with
 * the server, which should live as long a a `struct output`.
 *
 * @requires a fully initializes vdbwm_server with a working backend
 */
void init_output_manager(output_manager*, vdbwm_server*);

/**
 * Cleans up the output manager's internal state
 */
void destroy_output_manager(output_manager*);

#endif
