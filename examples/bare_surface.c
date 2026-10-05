// Minimal Wayland client that only uses wl_compositor and wl_shm (no xdg_shell).
// It creates a role-less wl_surface, attaches a solid red shm buffer and commits it.
//
// Build: gcc examples/bare_surface.c $(pkg-config --cflags --libs wayland-client) -o bare_surface
// Run:   WAYLAND_DISPLAY=wayland-1 ./bare_surface

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#define WIDTH 256
#define HEIGHT 256

static struct wl_compositor *compositor;
static struct wl_shm *shm;

static void registry_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static struct wl_buffer *create_buffer(void) {
    int stride = WIDTH * 4;
    int size = stride * HEIGHT;

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
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        pixels[i] = 0xffff0000; // opaque red in ARGB8888
    }
    munmap(pixels, size);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
            WIDTH, HEIGHT, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    return buffer;
}

int main(void) {
    struct wl_display *display = wl_display_connect(NULL);
    if (display == NULL) {
        fprintf(stderr, "failed to connect to Wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (compositor == NULL || shm == NULL) {
        fprintf(stderr, "compositor does not advertise wl_compositor and wl_shm\n");
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    wl_surface_attach(surface, create_buffer(), 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, WIDTH, HEIGHT);
    wl_surface_commit(surface);

    while (wl_display_dispatch(display) != -1) {
    }

    wl_display_disconnect(display);
    return 0;
}
