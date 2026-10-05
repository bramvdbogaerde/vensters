/** 
 * This module manages requests from clients to create new windows through the xdg-shell
 * protocol, positions them and resizes them into a tiling window layout 
 */
#ifndef WINDOW_H 
#define WINDOW_H

#include "server.h"

typedef struct {
    vdbwm_server* _srv;
    struct wlr_xdg_shell* _wlr_xdg_shell;
    vlistener _create_surface;
    vlistener _destroy_xdg_shell;
    vlistener _toplevel_connect;
} window_manager;

/** 
 * Initialize the window manager
 */
void init_window_manager(window_manager*, vdbwm_server*);

/** 
 * Destroy the window manager and clean up its resources.
 */
void destroy_window_manager(window_manager*);

#endif
