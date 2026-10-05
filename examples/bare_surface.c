// Minimal Wayland client that shows a solid red xdg_toplevel using wl_shm.
// It binds wl_compositor, wl_shm and xdg_wm_base, waits for the compositor's
// first configure, and then attaches a buffer of the requested size (or
// 256x256 if the compositor leaves the size up to the client).
//
// Build: make examples
// Run:   WAYLAND_DISPLAY=wayland-1 ./examples/bare_surface

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#define DEFAULT_WIDTH 256
#define DEFAULT_HEIGHT 256

static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct xdg_wm_base *wm_base;

static struct wl_surface *surface;
static int width = DEFAULT_WIDTH, height = DEFAULT_HEIGHT;
static int pending_width, pending_height;
static bool running = true;

////////////////////////////////////////////////////////////
// Buffers
////////////////////////////////////////////////////////////

static void buffer_release(void *data, struct wl_buffer *buffer) {
    // The compositor no longer reads from this buffer. We draw a fresh
    // buffer on every resize, so old ones can simply be destroyed.
    wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

static struct wl_buffer *create_buffer(int w, int h) {
    int stride = w * 4;
    int size = stride * h;

    int fd = memfd_create("bare_surface", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, size) < 0) {
        perror("shm");
        exit(1);
    }

    uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    for (int i = 0; i < w * h; i++) {
        pixels[i] = 0xffff0000; // opaque red in ARGB8888
    }
    munmap(pixels, size);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
            w, h, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    wl_buffer_add_listener(buffer, &buffer_listener, NULL);
    return buffer;
}

////////////////////////////////////////////////////////////
// xdg-shell
////////////////////////////////////////////////////////////

static void wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial) {
    // Compositors use ping/pong to detect unresponsive clients.
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping,
};

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
        int32_t w, int32_t h, struct wl_array *states) {
    // A size of 0 means the compositor lets the client pick. The values only
    // take effect once the xdg_surface.configure that follows arrives.
    pending_width = w;
    pending_height = h;
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    running = false;
}

static void toplevel_configure_bounds(void *data, struct xdg_toplevel *toplevel,
        int32_t w, int32_t h) {}

static void toplevel_wm_capabilities(void *data, struct xdg_toplevel *toplevel,
        struct wl_array *capabilities) {}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close,
    .configure_bounds = toplevel_configure_bounds,
    .wm_capabilities = toplevel_wm_capabilities,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    // xdg_surface.configure ends a configure sequence: acknowledge it and
    // commit a buffer that matches the requested state.
    xdg_surface_ack_configure(xdg_surface, serial);

    if (pending_width > 0 && pending_height > 0) {
        width = pending_width;
        height = pending_height;
    }

    wl_surface_attach(surface, create_buffer(width, height), 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, width, height);
    wl_surface_commit(surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

////////////////////////////////////////////////////////////
// Registry
////////////////////////////////////////////////////////////

static void registry_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main(void) {
    struct wl_display *display = wl_display_connect(NULL);
    if (display == NULL) {
        fprintf(stderr, "failed to connect to Wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (compositor == NULL || shm == NULL || wm_base == NULL) {
        fprintf(stderr, "compositor does not advertise wl_compositor, wl_shm and xdg_wm_base\n");
        return 1;
    }

    surface = wl_compositor_create_surface(compositor);
    struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    struct xdg_toplevel *toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_add_listener(toplevel, &toplevel_listener, NULL);
    xdg_toplevel_set_title(toplevel, "bare_surface");
    xdg_toplevel_set_app_id(toplevel, "bare_surface");

    // The initial commit has no buffer attached; it asks the compositor to
    // send the first configure. The buffer is attached in xdg_surface_configure.
    wl_surface_commit(surface);

    while (running && wl_display_dispatch(display) != -1) {
    }

    xdg_toplevel_destroy(toplevel);
    xdg_surface_destroy(xdg_surface);
    wl_surface_destroy(surface);
    wl_display_disconnect(display);
    return 0;
}
