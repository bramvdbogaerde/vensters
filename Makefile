source_files = $(shell find src -name '*.c')
object_files = $(source_files:.c=.o)
packages = wlroots-0.20 wayland-server
ld_libs = $(shell pkg-config --libs $(packages))
deps = $(object_files:.o=.d)

CXXFLAGS += -MMD -MP $(shell pkg-config --cflags $(packages)) -DWLR_USE_UNSTABLE
.PHONY: clean examples

vdbwm: $(object_files)
	gcc $(ld_libs) $^ -o vdbwm

examples: examples/bare_surface

examples/bare_surface: examples/bare_surface.c
	gcc $< $(shell pkg-config --cflags --libs wayland-client) -o $@

-include $(deps)

%.o: %.c
	gcc $(CXXFLAGS) -c $< -o $@

clean: 
	rm -f $(object_files) $(deps) examples/bare_surface
