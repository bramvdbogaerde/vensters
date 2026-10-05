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

wayland_protocols = $(shell pkg-config --variable=pkgdatadir wayland-protocols)
wayland_scanner = $(shell pkg-config --variable=wayland_scanner wayland-scanner)
xdg_shell_xml = $(wayland_protocols)/stable/xdg-shell/xdg-shell.xml

examples/xdg-shell-client-protocol.h:
	$(wayland_scanner) client-header $(xdg_shell_xml) $@

examples/xdg-shell-protocol.c:
	$(wayland_scanner) private-code $(xdg_shell_xml) $@

examples/bare_surface: examples/bare_surface.c examples/xdg-shell-protocol.c examples/xdg-shell-client-protocol.h
	gcc -Iexamples examples/bare_surface.c examples/xdg-shell-protocol.c $(shell pkg-config --cflags --libs wayland-client) -o $@

-include $(deps)

%.o: %.c
	gcc $(CXXFLAGS) -c $< -o $@

clean: 
	rm -f $(object_files) $(deps) examples/bare_surface examples/xdg-shell-protocol.c examples/xdg-shell-client-protocol.h
