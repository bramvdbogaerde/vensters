#include <stdlib.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include "server.h"
#include "output.h"
#include "window.h"

int main() {
    wlr_log_init(WLR_DEBUG, NULL);
    int exit_code = 0;
    
    vdbwm_server srv = {0};
    srv.wl_display = wl_display_create();
    
    // The code below is setup code to get a basic Wayland compistor running.
    srv.wlr_backend = wlr_backend_autocreate(wl_display_get_event_loop(srv.wl_display), NULL);
    // ^ the backend is used to connect to an appropriate graphical backend. If executed in a TTY 
    // or launched through a Wayland compatible display manager (such as GDM) it will connect
    // through kernel modesetting and use DRM (direct rendering manager) to provide surfaces and 
    // perform rendering of Wayland clients. 
    if (srv.wlr_backend == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_backend");
        return 1;
    }
    
    srv.wlr_renderer = wlr_renderer_autocreate(srv.wlr_backend);
    // ^ determines which renderer to use for composing windows.
    if (srv.wlr_renderer == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_renderer");
        return 1;
    }

    wlr_renderer_init_wl_display(srv.wlr_renderer, srv.wl_display);
    
    // The allocator actually implements the allocation logic for creating memory buffers
    // to be passed to clients for surfaces and subsurfaces.
    srv.wlr_allocator = wlr_allocator_autocreate(srv.wlr_backend, srv.wlr_renderer);
    if (srv.wlr_allocator == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_allocator");
        return 1;
    }
    
    // Initialize some core Wayland protocols for providing surfaces to clients
    srv.wlr_compositor = wlr_compositor_create(srv.wl_display, 5, srv.wlr_renderer);
    wlr_subcompositor_create(srv.wl_display);
        
    // Start litening on the wayland unix socket
    const char *socket = wl_display_add_socket_auto(srv.wl_display);
    if (!socket) {
        wlr_backend_destroy(srv.wlr_backend);
        return 1;
    }   

    // Managing changing outputs
    output_manager output_mgr;
    init_output_manager(&output_mgr, &srv);

    // Create a new layout
    srv.wlr_output_layout = wlr_output_layout_create(srv.wl_display);

    // Scene graph management, the Scene Graph API allows us to manage
    // clients easily in a graph structure and wlroots handles all of the 
    // layering, damage tracking, ...
    srv.wlr_scene = wlr_scene_create();

    // the scene's layout is used to manage outputs (i.e., multiple screens). 
    // Conceptually, the scene provides one global coordinate space in which
    // output are positioned. This allows wlroots to calculate things like mouse
    // movement across this coordinate space. The layout determines where each
    // output is positioned, and thus, what parts of a surface gets rendered on
    // each output.
    srv.wlr_scene_layout = wlr_scene_attach_output_layout(srv.wlr_scene, srv.wlr_output_layout);

    // Managing windows
    window_manager window_mgr;
    init_window_manager(&window_mgr, &srv);

    // Then start the backend
    if (!wlr_backend_start(srv.wlr_backend)) {
        wlr_backend_destroy(srv.wlr_backend);
        wl_display_destroy(srv.wl_display);
        return 1;
    }

    setenv("WAYLAND_DISPLAY", socket, true);
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", "/usr/bin/kitty", NULL);
    }

    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);

    // Finally start the Wayland event loop
    wl_display_run(srv.wl_display);

cleanup:
    wlr_scene_node_destroy(&srv.wlr_scene->tree.node); 
    wl_display_destroy_clients(srv.wl_display);
    wlr_allocator_destroy(srv.wlr_allocator);
    wlr_renderer_destroy(srv.wlr_renderer);
    destroy_output_manager(&output_mgr);
    wlr_backend_destroy(srv.wlr_backend);
    wl_display_destroy(srv.wl_display);
    return exit_code;
}
