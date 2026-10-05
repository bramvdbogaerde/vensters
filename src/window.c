#include "window.h"
#include "server.h"
#include <wlr/types/wlr_compositor.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_scene.h>

////////////////////////////////////////////////////////////
// Listeners 
////////////////////////////////////////////////////////////

DEFINE_LISTENER(create_surface, window_manager, struct wlr_surface);
static void create_surface(window_manager* mgr, struct wlr_surface* surface) {
    // wlroots already does most of the heavy lifting to create the appropriate 
    // buffers, we only need to add the surface to our scene.
    vdbwm_server* srv = mgr->_srv;
    struct wlr_scene_tree *tree = wlr_scene_subsurface_tree_create(&srv->wlr_scene->tree, surface);
    wlr_scene_node_set_enabled(&tree->node, true);
}

////////////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////////////

void init_window_manager(window_manager* mgr, vdbwm_server* srv) {
    mgr->_srv = srv;

    // Listen for requests for new surfaces
    create_surface_connect(&mgr->_create_surface, mgr, &srv->wlr_compositor->events.new_surface);
}   

void destroy_window_manager(window_manager* mgr) {
    create_surface_disconnect(&mgr->_create_surface);
}
