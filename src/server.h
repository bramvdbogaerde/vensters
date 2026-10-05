#ifndef SERVER_H
#define SERVER_H

#include <wayland-server-core.h>

typedef struct {
    struct wl_display* wl_display;
    struct wlr_backend *wlr_backend;
    struct wlr_renderer *wlr_renderer;
    struct wlr_allocator *wlr_allocator;
    struct wlr_output_layout *wlr_output_layout;
    struct wlr_compositor *wlr_compositor;
    struct wlr_scene *wlr_scene;
    struct wlr_scene_output_layout *wlr_scene_layout;
} vdbwm_server;

typedef struct {
    struct wl_listener base;
    void* closure;
} vlistener;

// Convenience macro to define listeners. It is meant to replace
// the manual plumbing to obtain the closure of the callback registered
// before. It does assume that the vlistener and closure pointer passed to name##_connect
// stay valid until the listener is removed.
#define DEFINE_LISTENER(name, closure_type, data_type) \
    static void name(closure_type*, data_type*); \
    static void name##_callback(struct wl_listener *l, void *data) { \
        vlistener *vl = wl_container_of(l, vl, base); \
        name(vl->closure, data); \
    } \
    static inline void name##_connect(vlistener* vl, closure_type *clo, struct wl_signal *signal) { \
        vl->closure = clo; \
        vl->base.notify = name##_callback; \
        wl_signal_add(signal, &vl->base); \
    } \
    static inline void name##_disconnect(vlistener *vl) { \
        wl_list_remove(&vl->base.link); \
    }

#endif
