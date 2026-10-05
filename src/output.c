#include "output.h"
#include "server.h"
#include "wlr/util/log.h"
#include <bits/time.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

////////////////////////////////////////////////////////////
// Internal struct
////////////////////////////////////////////////////////////

typedef struct {
    struct wl_list link;
    output_manager* mgr;
    struct wlr_output* output;
    vlistener destroy_output;
    vlistener output_frame;
} voutput;

////////////////////////////////////////////////////////////
// Listeners
////////////////////////////////////////////////////////////

DEFINE_LISTENER(output_frame, voutput, void);
DEFINE_LISTENER(destroy_output, voutput, void);

static void destroy_output(voutput* output, void* data) {
    // TODO: memory cleanup for now, although we probably also want 
    // to reorganize internally where all windows are located.
    destroy_output_disconnect(&output->destroy_output);
    output_frame_disconnect(&output->output_frame);
    wl_list_remove(&output->link);
    free(output);
}

static void output_frame(voutput* output, void* data) {
    struct wlr_scene_output* scene_output =
         wlr_scene_get_scene_output(output->mgr->_server->wlr_scene, output->output);

    wlr_scene_output_commit(scene_output, NULL);
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

DEFINE_LISTENER(new_output, output_manager, struct wlr_output);
static void new_output(output_manager* mgr, struct wlr_output* output) {
    vdbwm_server *srv = mgr->_server;

    // Configure the output to use our renderer and backend.
    wlr_output_init_render(output, srv->wlr_allocator, srv->wlr_renderer);

    // Create an initial state (i.e., the buffer) that we will use for this
    // output.
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    // Next configure the output and set its mode (i.e., which resolution, 
    // refresh rate, ...)
    struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
    if (mode != NULL) {
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);

    // Register the output here as well so that we can manage it when
    // it gets disconnect, resizes, ...
    voutput* our_output = malloc(sizeof(voutput));
    if (our_output == NULL) {
        wlr_log(WLR_ERROR, "could not allocated output");
        exit(1);
    }

    our_output->mgr = mgr;
    our_output->output = output;

    // Free up resources if the output is disconnect and reorganise windows
    destroy_output_connect(
            &our_output->destroy_output, 
            our_output, 
            &output->events.destroy
    );

    // Let's add the output to the output layout, and 
    // to the scene. TODO we should probably let the user 
    // configure *where* in the layout the output is 
    // supposed to be.
    struct wlr_output_layout_output* output_layout_output = 
        wlr_output_layout_add_auto(srv->wlr_output_layout, output);
    // ^ attaches the output to the output layout
    struct wlr_scene_output* scene_output =
        wlr_scene_output_create(srv->wlr_scene, output);
    // ^ creates an new output for the scene
    wlr_scene_output_layout_add_output(
            srv->wlr_scene_layout, 
            output_layout_output, 
            scene_output
    );
    // ^ adds the new output to the scene

    // Callback for when we are ready to display new frames,
    // this is were the drawing actually happens.
    output_frame_connect(&our_output->output_frame, our_output, &output->events.frame);

    // TODO: register callbacks for new frames
    // TODO: do some scene management to place the output into the scene somewhere
    wl_list_insert(&mgr->_outputs, &our_output->link);
}

////////////////////////////////////////////////////////////
// Private functions
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////////////

void destroy_output_manager(output_manager* mgr) {
    new_output_disconnect(&mgr->_new_output_listener);
}

void init_output_manager(output_manager* mgr, vdbwm_server* srv) {
    mgr->_new_output_listener = (vlistener) {0};
    mgr->_server = srv;
    wl_list_init(&mgr->_outputs);
    new_output_connect(&mgr->_new_output_listener, mgr, &srv->wlr_backend->events.new_output);
}
