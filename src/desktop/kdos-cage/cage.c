/*
 * kdos-cage: one application, full screen, on a VT of its own.
 *
 * A HARD FORK OF cage 0.3.1 (github.com/cage-kiosk/cage), which is MIT.
 * Upstream is a Wayland kiosk compositor built on wlroots 0.20 — the branch
 * this tree already pins for the compositor fork — and running one application
 * full screen while refusing every attempt to do anything else is exactly the
 * job. It was taken rather than written.
 *
 * PINNED, NOT TRACKED, in the sense kdos-comp is: this does not rebase onto
 * upstream.
 *
 * WHAT THE FORK CHANGED
 *
 *   The name, in what a person sees: the binary, the usage text and the
 *   version line. Upstream's INTERNAL names are left alone — a fork whose
 *   identifiers stop matching upstream's is a fork nobody can read a
 *   security fix against.
 *
 *   THE BACKGROUND IS THE PALETTE'S. Unpainted scene is black, and a black
 *   rectangle in the middle of a phosphor desktop reads as a dead screen
 *   rather than as an application that has not drawn yet. libkcolor is the
 *   only KDOS library this links.
 *
 *   XWayland stays. A boxed application may be an X11 client, and the rootless
 *   X server is this distribution's one carve-out.
 *
 * Copyright (C) 2018-2020 Jente Hidskes
 * Copyright (c) 2019 The Sway authors
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200112L

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/config.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_security_context_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#if CAGE_HAS_XWAYLAND
#include <wlr/types/wlr_xcursor_manager.h>
#endif
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#if CAGE_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "kcolor.h"

#include "clipboard.h"
#include "embed.h"
#include "idle_inhibit_v1.h"
#include "kembed.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "view.h"
#include "xdg_shell.h"
#if CAGE_HAS_XWAYLAND
#include "xwayland.h"
#endif

void
server_terminate(struct cg_server *server)
{
	// Workaround for https://gitlab.freedesktop.org/wayland/wayland/-/merge_requests/421
	if (server->terminated) {
		return;
	}

	wl_display_terminate(server->wl_display);
}

static void
handle_display_destroy(struct wl_listener *listener, void *data)
{
	struct cg_server *server = wl_container_of(listener, server, display_destroy);
	server->terminated = true;
}

#if WLR_HAS_DRM_BACKEND
static void
handle_drm_lease_request(struct wl_listener *listener, void *data)
{
	struct wlr_drm_lease_request_v1 *req = data;
	struct wlr_drm_lease_v1 *lease = wlr_drm_lease_request_v1_grant(req);
	if (!lease) {
		wlr_log(WLR_ERROR, "Failed to grant lease");
		wlr_drm_lease_request_v1_reject(req);
	}
}
#endif

static int
sigchld_handler(int fd, uint32_t mask, void *data)
{
	struct cg_server *server = data;

	/* Close the compositor's read pipe. */
	close(fd);

	if (mask & WL_EVENT_HANGUP) {
		wlr_log(WLR_DEBUG, "Child process closed normally");
	} else if (mask & WL_EVENT_ERROR) {
		wlr_log(WLR_DEBUG, "Connection closed by server");
	}

	server->return_app_code = true;
	server_terminate(server);
	return 0;
}

static bool
set_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);

	if (flags == -1) {
		wlr_log(WLR_ERROR, "Unable to set the CLOEXEC flag: fnctl failed");
		return false;
	}

	flags = flags | FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, flags) == -1) {
		wlr_log(WLR_ERROR, "Unable to set the CLOEXEC flag: fnctl failed");
		return false;
	}

	return true;
}

static bool
spawn_primary_client(struct cg_server *server, char *argv[], pid_t *pid_out, struct wl_event_source **sigchld_source)
{
	int fd[2];
	if (pipe(fd) != 0) {
		wlr_log(WLR_ERROR, "Unable to create pipe");
		return false;
	}

	pid_t pid = fork();
	if (pid == 0) {
		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, NULL);
		/* Close read, we only need write in the primary client process. */
		close(fd[0]);
		execvp(argv[0], argv);
		/* execvp() returns only on failure */
		wlr_log_errno(WLR_ERROR, "Failed to spawn client");
		_exit(1);
	} else if (pid == -1) {
		wlr_log_errno(WLR_ERROR, "Unable to fork");
		return false;
	}

	/* Set this early so that if we fail, the client process will be cleaned up properly. */
	*pid_out = pid;

	if (!set_cloexec(fd[0]) || !set_cloexec(fd[1])) {
		return false;
	}

	/* Close write, we only need read in the compositor. */
	close(fd[1]);

	struct wl_event_loop *event_loop = wl_display_get_event_loop(server->wl_display);
	uint32_t mask = WL_EVENT_HANGUP | WL_EVENT_ERROR;
	*sigchld_source = wl_event_loop_add_fd(event_loop, fd[0], mask, sigchld_handler, server);

	wlr_log(WLR_DEBUG, "Child process created with pid %d", pid);
	return true;
}

/*
 * HOW LONG THE GUEST HAS TO GO, and then how long it has after being told.
 *
 * The first window is the ordinary quit: the display has terminated, the
 * client's connection is gone and an application notices and exits. The second
 * is what a container runtime with children of its own takes to unwind.
 */
#define CAGE_CLEANUP_TERM_MS 3000
#define CAGE_CLEANUP_KILL_MS 2000

/*
 * BOUNDED, AND IT ESCALATES.
 *
 * A guest that does not exit would otherwise hold this process in waitpid()
 * for ever — and the signals that would interrupt it cannot arrive, because
 * the event loop's signal sources are signalfds and the loop blocked SIGINT
 * and SIGTERM to create them. A cage stuck here is a window on the parent's
 * desktop that nothing can close and a box that nothing can collect.
 */
static int
cleanup_primary_client(pid_t pid)
{
	int status = 0;

	for (int ms = 0; ms < CAGE_CLEANUP_TERM_MS; ms += 20) {
		pid_t r = waitpid(pid, &status, WNOHANG);

		if (r == pid || (r < 0 && errno != EINTR)) {
			goto reaped;
		}
		nanosleep(&(struct timespec){ .tv_nsec = 20 * 1000 * 1000 },
			  NULL);
	}
	kill(pid, SIGTERM);
	for (int ms = 0; ms < CAGE_CLEANUP_KILL_MS; ms += 20) {
		pid_t r = waitpid(pid, &status, WNOHANG);

		if (r == pid || (r < 0 && errno != EINTR)) {
			goto reaped;
		}
		nanosleep(&(struct timespec){ .tv_nsec = 20 * 1000 * 1000 },
			  NULL);
	}
	kill(pid, SIGKILL);
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
		;
	}

reaped:
	if (WIFEXITED(status)) {
		wlr_log(WLR_DEBUG, "Child exited normally with exit status %d", WEXITSTATUS(status));
		return WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		/* Mimic Bash and other shells for the exit status */
		wlr_log(WLR_DEBUG, "Child was terminated by a signal (%d)", WTERMSIG(status));
		return 128 + WTERMSIG(status);
	}

	return 0;
}

static bool
drop_permissions(void)
{
	if (getuid() == 0 || getgid() == 0) {
		wlr_log(WLR_INFO, "Running as root user, this is dangerous");
		return true;
	}
	if (getuid() != geteuid() || getgid() != getegid()) {
		wlr_log(WLR_INFO, "setuid/setgid bit detected, dropping permissions");
		// Set the gid and uid in the correct order.
		if (setgid(getgid()) != 0 || setuid(getuid()) != 0) {
			wlr_log(WLR_ERROR, "Unable to drop root, refusing to start");
			return false;
		}
	}

	if (setgid(0) != -1 || setuid(0) != -1) {
		wlr_log(WLR_ERROR,
			"Unable to drop root (we shouldn't be able to restore it after setuid), refusing to start");
		return false;
	}

	return true;
}

static int
handle_signal(int signal, void *data)
{
	struct cg_server *server = data;

	switch (signal) {
	case SIGINT:
		/* Fallthrough */
	case SIGTERM:
		server_terminate(server);
		return 0;
	default:
		return 0;
	}
}

/*
 * The headless backend inside whatever autocreate built. It wraps a single
 * backend in a MULTI one, and an output has to be added to the real thing —
 * the wrapper asserts rather than forwarding.
 */
static void
find_headless_iter(struct wlr_backend *backend, void *data)
{
	struct wlr_backend **out = data;

	if (!*out && wlr_backend_is_headless(backend))
		*out = backend;
}

static struct wlr_backend *
find_headless(struct wlr_backend *backend)
{
	if (wlr_backend_is_headless(backend))
		return backend;

	struct wlr_backend *found = NULL;

	if (wlr_backend_is_multi(backend))
		wlr_multi_for_each_backend(backend, find_headless_iter, &found);
	return found;
}

static void
usage(FILE *file, const char *cage)
{
	fprintf(file,
		"Usage: %s [OPTIONS] [--] [APPLICATION...]\n"
		"\n"
		" -d\t Don't draw client side decorations, when possible\n"
		" -D\t Enable debug logging\n"
		" -h\t Display this help message\n"
		" -m extend Extend the display across all connected outputs (default)\n"
		" -m last Use only the last connected output\n"
		" -s\t Allow VT switching\n"
		" -v\t Show the version number and exit\n"
		"\n"
		" --embed WxH  Render into a shared buffer at this pixel size\n"
		"              instead of onto a screen, and take input from the\n"
		"              parent. Started by kdos-con; the channel is fd 3\n"
		"\n"
		" Use -- when you want to pass arguments to APPLICATION\n",
		cage);
}

/*
 * THE BACKGROUND, from the palette. A scene rectangle at the bottom of the
 * tree and behind everything else, so an application that has not painted yet
 * is the desktop's own deep colour rather than a black hole in the middle of
 * a phosphor screen.
 *
 * Resized whenever the layout changes, because a rectangle sized once is a
 * rectangle that stops covering the screen the moment a monitor is plugged in.
 */
static void
background_color(float out[4])
{
	char name[64];
	const KcolScheme *sc = NULL;

	if (kcol_theme_name(name, sizeof(name)))
		sc = kcol_find(name);
	if (!sc)
		sc = &kcol_schemes[0];

	out[0] = (float)((sc->deep >> 16) & 0xff) / 255.0f;
	out[1] = (float)((sc->deep >> 8) & 0xff) / 255.0f;
	out[2] = (float)(sc->deep & 0xff) / 255.0f;
	out[3] = 1.0f;
}

static bool
parse_args(struct cg_server *server, int argc, char *argv[])
{
	/*
	 * --embed IS A LONG OPTION AND THE ONLY ONE, because it is not a thing
	 * a person types: kdos-con passes it, with the channel already on
	 * fd 3. getopt_long with one entry keeps the short options upstream's.
	 */
	static const struct option longs[] = {
		{ "embed", required_argument, NULL, 'E' },
		{ NULL, 0, NULL, 0 },
	};
	int c;
	while ((c = getopt_long(argc, argv, "dDhm:sv", longs, NULL)) != -1) {
		switch (c) {
		case 'd':
			server->xdg_decoration = true;
			break;
		case 'D':
			server->log_level = WLR_DEBUG;
			break;
		case 'h':
			usage(stdout, argv[0]);
			return false;
		case 'm':
			if (strcmp(optarg, "last") == 0) {
				server->output_mode = CAGE_MULTI_OUTPUT_MODE_LAST;
			} else if (strcmp(optarg, "extend") == 0) {
				server->output_mode = CAGE_MULTI_OUTPUT_MODE_EXTEND;
			}
			break;
		case 'E': {
			int w = 0, h = 0;

			if (sscanf(optarg, "%dx%d", &w, &h) != 2 || w < 1 ||
			    h < 1 || w > CG_EMBED_SPAN || h > CG_EMBED_SPAN) {
				fprintf(stderr, "kdos-cage: --embed wants WxH\n");
				return false;
			}
			server->embed.first_w = w;
			server->embed.first_h = h;
			server->embed.fd = KEMBED_FD;
			/* Not `active` yet: that is embed_init's to set, once
			 * the event source is armed and the parent has been
			 * told. `embedded` is what the layout and the focus
			 * rules read, and it has to be true from here. */
			server->embed.embedded = true;
			break;
		}
		case 's':
			server->allow_vt_switch = true;
			break;
		case 'v':
			fprintf(stdout, "kdos-cage " CAGE_VERSION
					" (a fork of cage " CAGE_UPSTREAM ")\n");
			exit(0);
		default:
			usage(stderr, argv[0]);
			return false;
		}
	}

	return true;
}

int
main(int argc, char *argv[])
{
	struct cg_server server = {.log_level = WLR_INFO};
	struct wl_event_source *sigchld_source = NULL;
	pid_t pid = 0;
	int ret = 0, app_ret = 0;

#ifdef DEBUG
	server.log_level = WLR_DEBUG;
#endif

	if (!parse_args(&server, argc, argv)) {
		return 1;
	}

	wlr_log_init(server.log_level, NULL);

	/* Wayland requires XDG_RUNTIME_DIR to be set. */
	if (!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR is not set in the environment");
		return 1;
	}

	server.wl_display = wl_display_create();
	if (!server.wl_display) {
		wlr_log(WLR_ERROR, "Cannot allocate a Wayland display");
		return 1;
	}

	wl_display_set_default_max_buffer_size(server.wl_display, 1024 * 1024);
	server.display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(server.wl_display, &server.display_destroy);

	struct wl_event_loop *event_loop = wl_display_get_event_loop(server.wl_display);
	struct wl_event_source *sigint_source = wl_event_loop_add_signal(event_loop, SIGINT, handle_signal, &server);
	struct wl_event_source *sigterm_source = wl_event_loop_add_signal(event_loop, SIGTERM, handle_signal, &server);

	/*
	 * EMBEDDED IS HEADLESS PLUS PIXMAN, and both are chosen the way wlroots
	 * already lets anyone choose them. A headless output is backed by a
	 * buffer in memory rather than by a screen, and the software renderer
	 * is the one that can hand back a pointer to those bytes — that pair is
	 * the entire mechanism.
	 *
	 * Set rather than hand-built, so the code path is upstream's own and a
	 * wlroots that changes how a backend is made changes it here too. They
	 * are set only if the environment has not already: a person debugging
	 * with WLR_BACKENDS set means it.
	 */
	if (server.embed.embedded) {
		setenv("WLR_BACKENDS", "headless", 0);
		/*
		 * THE SOFTWARE RENDERER UNLESS THE APPLICATION ASKED FOR THE
		 * CARD. Pixman draws into memory this process can read with no
		 * device, no driver and nothing to negotiate, which is the
		 * right trade for an editor and the wrong one for a game: a
		 * guest under it has no hardware GL, no hardware video decode
		 * and no dmabuf to offer. `render = gpu` in the box profile is
		 * what asks for the other one, and it reaches this process as
		 * KDOS_EMBED_GPU; see embed_gpu_setup().
		 */
		setenv("WLR_RENDERER",
		       getenv("KDOS_EMBED_GPU") ? "gles2" : "pixman", 0);
		/*
		 * NONE OF ITS OWN. autocreate adds headless outputs at a size
		 * of its choosing, and a second output beside the one this mode
		 * adds is a layout twice as wide as the window — which is
		 * exactly what the frames then are. Embedded is one window and
		 * therefore one output.
		 */
		setenv("WLR_HEADLESS_OUTPUTS", "0", 0);
		/*
		 * THE CURSOR HAS TO BE IN THE PICTURE, because the picture is
		 * all the parent gets. A headless output answers set_cursor
		 * and move_cursor with `true` and stores nothing, so wlroots
		 * believes a hardware plane carries the cursor and leaves it
		 * out of the render pass — and the parent, which reads the
		 * rendered bytes and nothing else, has no plane to composite.
		 * The guest's pointer would be invisible and its shape, which
		 * is how an application says what is under it, would never
		 * arrive. Overridden rather than defaulted: there is no
		 * headless cursor plane to prefer, so a person who set this to
		 * 0 set it for some other compositor.
		 */
		setenv("WLR_NO_HARDWARE_CURSORS", "1", 1);
	}

	server.backend = wlr_backend_autocreate(event_loop, &server.session);
	if (!server.backend) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots backend");
		ret = 1;
		goto end;
	}

	/*
	 * A headless backend has no outputs of its own — one is added at the
	 * size the parent asked for, and a window resize resizes it.
	 *
	 * THIS IS THE ANCHOR AND IT IS NEVER DESTROYED. The first toplevel
	 * claims it, so a guest that shows one window allocates nothing and is
	 * configured at the size this cage was forked with; outputs for the
	 * second and further windows are added at the map. A cage that made its
	 * first output lazily would sit with none at all for as long as the
	 * guest takes to start — and a client that waits for a wl_output before
	 * mapping never maps, while Xwayland's root screen stays 0x0 where no X
	 * client can map either.
	 */
	if (server.embed.embedded) {
		server.headless = find_headless(server.backend);

		if (!server.headless ||
		    !wlr_headless_add_output(server.headless,
					     (unsigned int)server.embed.first_w,
					     (unsigned int)server.embed.first_h)) {
			wlr_log(WLR_ERROR, "Unable to create the embedded output");
			ret = 1;
			goto end;
		}
	}

	if (!drop_permissions()) {
		ret = 1;
		goto end;
	}

	server.renderer = wlr_renderer_autocreate(server.backend);
	if (!server.renderer && server.embed.embedded && getenv("KDOS_EMBED_GPU")) {
		wlr_log(WLR_INFO, "embed: no hardware renderer, using pixman");
		unsetenv("KDOS_EMBED_GPU");
		setenv("WLR_RENDERER", "pixman", 1);
		server.renderer = wlr_renderer_autocreate(server.backend);
	}
	if (!server.renderer) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots renderer");
		ret = 1;
		goto end;
	}

	server.allocator = server.embed.embedded
				   ? embed_allocator(server.backend,
						     server.renderer)
				   : wlr_allocator_autocreate(server.backend,
							      server.renderer);
	/*
	 * A HARDWARE RENDERER WITH NO READABLE BUFFER IS WORSE THAN NO
	 * HARDWARE RENDERER, because what it produces is a window that is
	 * black for ever while every other part of the mechanism reports
	 * success. Falling back costs this guest the card and nothing else.
	 */
	if (!server.allocator && server.embed.embedded && getenv("KDOS_EMBED_GPU")) {
		wlr_log(WLR_INFO, "embed: no readable buffer for the hardware "
				  "renderer, using pixman");
		unsetenv("KDOS_EMBED_GPU");
		setenv("WLR_RENDERER", "pixman", 1);
		wlr_renderer_destroy(server.renderer);
		server.renderer = wlr_renderer_autocreate(server.backend);
		if (server.renderer)
			server.allocator = embed_allocator(server.backend,
							   server.renderer);
	}
	if (!server.allocator) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots allocator");
		ret = 1;
		goto end;
	}

	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	/*
	 * WHAT LETS A CLIENT HAND OVER A BUFFER THE CARD ALREADY HOLDS.
	 *
	 * Without linux-dmabuf a client has one way to give this compositor a
	 * frame — shared memory — so a game renders on the GPU, reads the
	 * result back to the CPU and posts it, and a video is decoded in
	 * hardware only to be copied out of it. Advertised from what the
	 * renderer can actually sample, so a software renderer advertises
	 * nothing and a client keeps the road that works.
	 *
	 * The timeline manager goes with it: explicit synchronisation is how a
	 * client says a buffer is finished without the driver guessing, and a
	 * guess costs either a stall or a torn frame.
	 */
	if (wlr_renderer_get_texture_formats(server.renderer,
					     WLR_BUFFER_CAP_DMABUF)) {
		if (wlr_renderer_get_drm_fd(server.renderer) >= 0)
			wlr_drm_create(server.wl_display, server.renderer);
		wlr_linux_dmabuf_v1_create_with_renderer(server.wl_display, 5,
							 server.renderer);
	}
	if (wlr_renderer_get_drm_fd(server.renderer) >= 0 &&
	    server.renderer->features.timeline &&
	    server.backend->features.timeline)
		wlr_linux_drm_syncobj_manager_v1_create(
			server.wl_display, 1,
			wlr_renderer_get_drm_fd(server.renderer));

	wl_list_init(&server.views);
	wl_list_init(&server.outputs);

	server.output_layout = wlr_output_layout_create(server.wl_display);
	if (!server.output_layout) {
		wlr_log(WLR_ERROR, "Unable to create output layout");
		ret = 1;
		goto end;
	}
	server.output_layout_change.notify = handle_output_layout_change;
	wl_signal_add(&server.output_layout->events.change, &server.output_layout_change);

	server.scene = wlr_scene_create();
	if (!server.scene) {
		wlr_log(WLR_ERROR, "Unable to create scene");
		ret = 1;
		goto end;
	}

	server.scene_output_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

	/*
	 * BEHIND EVERYTHING, and created before any view so it is the first
	 * child of the tree: the scene draws children in order, and a
	 * background added later would be a background over the application.
	 * Zero-sized until the first layout change, which is when there is a
	 * screen to cover.
	 */
	float bg[4];

	background_color(bg);
	server.background = wlr_scene_rect_create(&server.scene->tree, 0, 0, bg);

	struct wlr_compositor *compositor = wlr_compositor_create(server.wl_display, 6, server.renderer);
	if (!compositor) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots compositor");
		ret = 1;
		goto end;
	}

	if (!wlr_subcompositor_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots subcompositor");
		ret = 1;
		goto end;
	}

	if (!wlr_data_device_manager_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create the data device manager");
		ret = 1;
		goto end;
	}

	/*
	 * SECURITY-CONTEXT-V1, so kdos-boxsock can tag this guest's socket the
	 * way it tags one under the graphical compositor. A boxed application
	 * reaches this desktop through exactly the same kdos-appbox launch, and
	 * a launch that worked on one desktop and failed on the other would be
	 * two launch paths to keep true instead of one.
	 *
	 * NO POLICY IS ATTACHED, and none is wanted: this compositor holds ONE
	 * client and shows it the whole screen. A tag exists here to identify,
	 * not to restrict — kdos-comp is where restricting a box's access to
	 * other windows means anything, because that is where other windows
	 * are.
	 */
	if (!wlr_security_context_manager_v1_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create the security context manager");
		ret = 1;
		goto end;
	}

	if (!wlr_primary_selection_v1_device_manager_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create primary selection device manager");
		ret = 1;
		goto end;
	}

	/* Configure a listener to be notified when new outputs are
	 * available on the backend. We use this only to detect the
	 * first output and ignore subsequent outputs. */
	server.new_output.notify = handle_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	server.seat = seat_create(&server, server.backend);
	if (!server.seat) {
		wlr_log(WLR_ERROR, "Unable to create the seat");
		ret = 1;
		goto end;
	}

	server.idle = wlr_idle_notifier_v1_create(server.wl_display);
	if (!server.idle) {
		wlr_log(WLR_ERROR, "Unable to create the idle tracker");
		ret = 1;
		goto end;
	}

	server.idle_inhibit_v1 = wlr_idle_inhibit_v1_create(server.wl_display);
	if (!server.idle_inhibit_v1) {
		wlr_log(WLR_ERROR, "Cannot create the idle inhibitor");
		ret = 1;
		goto end;
	}
	server.new_idle_inhibitor_v1.notify = handle_idle_inhibitor_v1_new;
	wl_signal_add(&server.idle_inhibit_v1->events.new_inhibitor, &server.new_idle_inhibitor_v1);
	wl_list_init(&server.inhibitors);

	struct wlr_xdg_shell *xdg_shell = wlr_xdg_shell_create(server.wl_display, 5);
	if (!xdg_shell) {
		wlr_log(WLR_ERROR, "Unable to create the XDG shell interface");
		ret = 1;
		goto end;
	}
	server.new_xdg_toplevel.notify = handle_new_xdg_toplevel;
	wl_signal_add(&xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
	server.new_xdg_popup.notify = handle_new_xdg_popup;
	wl_signal_add(&xdg_shell->events.new_popup, &server.new_xdg_popup);

	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager =
		wlr_xdg_decoration_manager_v1_create(server.wl_display);
	if (!xdg_decoration_manager) {
		wlr_log(WLR_ERROR, "Unable to create the XDG decoration manager");
		ret = 1;
		goto end;
	}
	wl_signal_add(&xdg_decoration_manager->events.new_toplevel_decoration, &server.xdg_toplevel_decoration);
	server.xdg_toplevel_decoration.notify = handle_xdg_toplevel_decoration;

	struct wlr_server_decoration_manager *server_decoration_manager =
		wlr_server_decoration_manager_create(server.wl_display);
	if (!server_decoration_manager) {
		wlr_log(WLR_ERROR, "Unable to create the server decoration manager");
		ret = 1;
		goto end;
	}
	wlr_server_decoration_manager_set_default_mode(
		server_decoration_manager, server.xdg_decoration ? WLR_SERVER_DECORATION_MANAGER_MODE_SERVER
								 : WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT);

	if (!wlr_viewporter_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create the viewporter interface");
		ret = 1;
		goto end;
	}

	struct wlr_presentation *presentation = wlr_presentation_create(server.wl_display, server.backend, 2);
	if (!presentation) {
		wlr_log(WLR_ERROR, "Unable to create the presentation interface");
		ret = 1;
		goto end;
	}

	if (!wlr_export_dmabuf_manager_v1_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create the export DMABUF manager");
		ret = 1;
		goto end;
	}

	if (!wlr_screencopy_manager_v1_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create the screencopy manager");
		ret = 1;
		goto end;
	}

	if (!wlr_single_pixel_buffer_manager_v1_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create the single pixel buffer manager");
		ret = 1;
		goto end;
	}

	if (!wlr_xdg_output_manager_v1_create(server.wl_display, server.output_layout)) {
		wlr_log(WLR_ERROR, "Unable to create the output manager");
		ret = 1;
		goto end;
	}

	server.output_manager_v1 = wlr_output_manager_v1_create(server.wl_display);
	if (!server.output_manager_v1) {
		wlr_log(WLR_ERROR, "Unable to create the output manager");
		ret = 1;
		goto end;
	}
	server.output_manager_apply.notify = handle_output_manager_apply;
	wl_signal_add(&server.output_manager_v1->events.apply, &server.output_manager_apply);
	server.output_manager_test.notify = handle_output_manager_test;
	wl_signal_add(&server.output_manager_v1->events.test, &server.output_manager_test);

#if WLR_HAS_DRM_BACKEND
	server.drm_lease_v1 = wlr_drm_lease_v1_manager_create(server.wl_display, server.backend);
	if (server.drm_lease_v1) {
		server.drm_lease_request.notify = handle_drm_lease_request;
		wl_signal_add(&server.drm_lease_v1->events.request, &server.drm_lease_request);
	} else {
		wlr_log(WLR_INFO, "Failed to create wlr_drm_lease_manager_v1");
	}
#endif

	if (!wlr_gamma_control_manager_v1_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create the gamma control manager");
		ret = 1;
		goto end;
	}

	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard =
		wlr_virtual_keyboard_manager_v1_create(server.wl_display);
	if (!virtual_keyboard) {
		wlr_log(WLR_ERROR, "Unable to create the virtual keyboard manager");
		ret = 1;
		goto end;
	}
	wl_signal_add(&virtual_keyboard->events.new_virtual_keyboard, &server.new_virtual_keyboard);

	struct wlr_virtual_pointer_manager_v1 *virtual_pointer =
		wlr_virtual_pointer_manager_v1_create(server.wl_display);
	if (!virtual_pointer) {
		wlr_log(WLR_ERROR, "Unable to create the virtual pointer manager");
		ret = 1;
		goto end;
	}
	wl_signal_add(&virtual_pointer->events.new_virtual_pointer, &server.new_virtual_pointer);

	server.relative_pointer_manager = wlr_relative_pointer_manager_v1_create(server.wl_display);
	if (!server.relative_pointer_manager) {
		wlr_log(WLR_ERROR, "Unable to create the relative pointer manager");
		ret = 1;
		goto end;
	}

	server.foreign_toplevel_manager = wlr_foreign_toplevel_manager_v1_create(server.wl_display);
	if (!server.foreign_toplevel_manager) {
		wlr_log(WLR_ERROR, "Unable to create the foreign toplevel manager");
		ret = 1;
		goto end;
	}

#if CAGE_HAS_XWAYLAND
	struct wlr_xcursor_manager *xcursor_manager = NULL;
	struct wlr_xwayland *xwayland = wlr_xwayland_create(server.wl_display, compositor, true);
	if (!xwayland) {
		wlr_log(WLR_ERROR, "Cannot create XWayland server");
	} else {
		server.new_xwayland_surface.notify = handle_xwayland_surface_new;
		wl_signal_add(&xwayland->events.new_surface, &server.new_xwayland_surface);

		xcursor_manager = wlr_xcursor_manager_create(NULL, XCURSOR_SIZE);
		if (!xcursor_manager) {
			wlr_log(WLR_ERROR, "Cannot create XWayland XCursor manager");
			ret = 1;
			goto end;
		}

		if (setenv("DISPLAY", xwayland->display_name, true) < 0) {
			wlr_log_errno(WLR_ERROR,
				      "Unable to set DISPLAY for XWayland. Clients may not be able to connect");
		} else {
			wlr_log(WLR_DEBUG, "XWayland is running on display %s", xwayland->display_name);
		}

		if (!wlr_xcursor_manager_load(xcursor_manager, 1)) {
			wlr_log(WLR_ERROR, "Cannot load XWayland XCursor theme");
		}
		struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(xcursor_manager, DEFAULT_XCURSOR, 1);
		if (xcursor) {
			struct wlr_xcursor_image *image = xcursor->images[0];
			wlr_xwayland_set_cursor(xwayland, wlr_xcursor_image_get_buffer(image), image->hotspot_x,
						image->hotspot_y);
		}
	}
#endif

	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_log_errno(WLR_ERROR, "Unable to open Wayland socket");
		ret = 1;
		goto end;
	}

	if (!wlr_backend_start(server.backend)) {
		wlr_log(WLR_ERROR, "Unable to start the wlroots backend");
		ret = 1;
		goto end;
	}

	if (setenv("WAYLAND_DISPLAY", socket, true) < 0) {
		wlr_log_errno(WLR_ERROR, "Unable to set WAYLAND_DISPLAY. Clients may not be able to connect");
	} else {
		wlr_log(WLR_DEBUG, "kdos-cage " CAGE_VERSION " is running on Wayland display %s", socket);
	}

#if CAGE_HAS_XWAYLAND
	if (xwayland) {
		wlr_xwayland_set_seat(xwayland, server.seat->seat);
	}
#endif

	if (optind < argc && !spawn_primary_client(&server, argv + optind, &pid, &sigchld_source)) {
		ret = 1;
		goto end;
	}

	/*
	 * THE CHANNEL COMES UP LAST, after the guest has been started: the
	 * first thing the parent hears is HELLO, and a parent that heard it
	 * before there was anything to draw would open an empty window.
	 */
	if (server.embed.embedded) {
		seat_embed_enable(server.seat);
		if (!embed_init(&server, server.embed.fd)) {
			wlr_log(WLR_ERROR, "Unable to open the embed channel");
			ret = 1;
			goto end;
		}
		/*
		 * THE SELECTION BRIDGE COMES UP WITH THE CHANNEL, because a
		 * copy this cage makes before it is listening is a copy the
		 * session never hears about — and it is the session that owns
		 * the clipboard every other window pastes from.
		 */
		clipboard_init(&server);
	}

	seat_center_cursor(server.seat);
	wl_display_run(server.wl_display);

	clipboard_finish(&server);
	embed_finish(&server);

#if CAGE_HAS_XWAYLAND
	if (xwayland) {
		wl_list_remove(&server.new_xwayland_surface.link);
	}
	wlr_xwayland_destroy(xwayland);
	wlr_xcursor_manager_destroy(xcursor_manager);
#endif
	wl_display_destroy_clients(server.wl_display);

#if WLR_HAS_DRM_BACKEND
	if (server.drm_lease_v1) {
		wl_list_remove(&server.drm_lease_request.link);
	}
#endif
	wl_list_remove(&server.new_virtual_pointer.link);
	wl_list_remove(&server.new_virtual_keyboard.link);
	wl_list_remove(&server.output_manager_apply.link);
	wl_list_remove(&server.output_manager_test.link);
	wl_list_remove(&server.xdg_toplevel_decoration.link);
	wl_list_remove(&server.new_xdg_toplevel.link);
	wl_list_remove(&server.new_xdg_popup.link);
	wl_list_remove(&server.new_idle_inhibitor_v1.link);
	wl_list_remove(&server.new_output.link);
	wl_list_remove(&server.output_layout_change.link);

end:
	if (pid != 0)
		app_ret = cleanup_primary_client(pid);
	if (!ret && server.return_app_code)
		ret = app_ret;

	wl_event_source_remove(sigint_source);
	wl_event_source_remove(sigterm_source);
	if (sigchld_source) {
		wl_event_source_remove(sigchld_source);
	}
	seat_destroy(server.seat);
	/* This function is not null-safe, but we only ever get here
	   with a proper wl_display. */
	wl_display_destroy(server.wl_display);
	if (server.scene != NULL) {
		wlr_scene_node_destroy(&server.scene->tree.node);
	}
	wlr_allocator_destroy(server.allocator);
	wlr_renderer_destroy(server.renderer);
	return ret;
}
