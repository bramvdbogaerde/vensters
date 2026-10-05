#include "window.h"
#include "server.h"
#include "wlr/types/wlr_xdg_decoration_v1.h"
#include "wlr/util/log.h"
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>

////////////////////////////////////////////////////////////
// Internal structs
////////////////////////////////////////////////////////////

typedef struct {
    window_manager* mgr;
    struct wlr_xdg_toplevel* toplevel;
    vlistener commit;
    vlistener destroy;
} vwindow;

////////////////////////////////////////////////////////////
// Listeners 
////////////////////////////////////////////////////////////

DEFINE_LISTENER(destroy_xdg_shell, window_manager, void);
DEFINE_LISTENER(toplevel_xdg_shell, window_manager, struct wlr_xdg_toplevel);
DEFINE_LISTENER(surface_commit, vwindow, void);
DEFINE_LISTENER(surface_destroy, vwindow, void);

static void destroy_xdg_shell(window_manager* mgr, void* data) {
    // Cleanup the xdg_shell listeners
    destroy_xdg_shell_disconnect(&mgr->_destroy_xdg_shell);
    toplevel_xdg_shell_disconnect(&mgr->_toplevel_connect);
}

// Toplevel windows

static void toplevel_xdg_shell(window_manager* mgr, struct wlr_xdg_toplevel* toplevel) {
    vdbwm_server* srv = mgr->_srv;
    // add the requested surface to the scene graph
    struct wlr_scene_tree* tree = wlr_scene_xdg_surface_create(&srv->wlr_scene->tree, toplevel->base);
    wlr_scene_node_set_position(&tree->node, 100, 100);
    // create an internally managed window for the client
    vwindow* win = malloc(sizeof(vwindow));
    if (win == NULL) {
        wlr_log(WLR_ERROR, "could not allocate window");
        return;
    }
    win->mgr = mgr;
    win->toplevel = toplevel;

    // wait for the client to commit, and then send its dimensions
    surface_commit_connect(&win->commit, win, &toplevel->base->surface->events.commit);
    surface_destroy_connect(&win->destroy, win, &toplevel->base->surface->events.destroy);
}

// Surfaces

static void surface_commit(vwindow* win, void* data) {
    if (win->toplevel->base->initial_commit) {
        wlr_xdg_toplevel_set_size(win->toplevel, 500, 500);
    }
}

static void surface_destroy(vwindow* win, void* data) {
    surface_commit_disconnect(&win->commit);
    surface_destroy_disconnect(&win->destroy);
    free(win);
}

////////////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////////////

void init_window_manager(window_manager* mgr, vdbwm_server* srv) {
    mgr->_srv = srv;

    // We will provide the xdg-shell protocol to let application request surfaces, request popups,
    // title bars, ...
    mgr->_wlr_xdg_shell = wlr_xdg_shell_create(mgr->_srv->wl_display, 7);
    destroy_xdg_shell_connect(&mgr->_destroy_xdg_shell, mgr, &mgr->_wlr_xdg_shell->events.destroy);

    // The clients raise a toplevel event to request a surface, this is simialr to what new_surface
    // provides in the core protocol, but also handles other things such as decorations, window titles,
    // activity (through a ping-pong interface, ...)
    toplevel_xdg_shell_connect(&mgr->_toplevel_connect, mgr, &mgr->_wlr_xdg_shell->events.new_toplevel);
}   

void destroy_window_manager(window_manager* mgr) {
}
