/*
 * embedcheck — the parent half of `kdos-cage --embed`, as a test.
 *
 * It is the smallest thing that can prove the mode works: fork kdos-cage with
 * the private channel on fd 3, take the shared mapping it sends, wait for a
 * frame, optionally type into it, and write what arrived as a PPM.
 *
 * WHY A SECOND PROCESS AND NOT A MOCK. The whole mechanism is a headless
 * wlroots output, a software renderer, a memfd and SCM_RIGHTS — every part of
 * which is a real kernel and library behaviour that a mock would assert about
 * itself. This drives the actual binary and looks at the actual pixels.
 */

/* libkbase-style: the flag is on the command line in the shipped build, so a
 * bare define here collides with it under -Werror. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kembed.h"

static int64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int recv_msg(int fd, KembedMsg *m, int *gotfd, int timeout_ms)
{
	struct iovec iov = { .iov_base = m, .iov_len = sizeof(*m) };
	struct msghdr hdr = { .msg_iov = &iov, .msg_iovlen = 1 };
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	struct pollfd p = { fd, POLLIN, 0 };

	*gotfd = -1;
	memset(&u, 0, sizeof(u));
	hdr.msg_control = u.buf;
	hdr.msg_controllen = sizeof(u.buf);

	if (poll(&p, 1, timeout_ms) <= 0)
		return 0;

	ssize_t n = recvmsg(fd, &hdr, 0);

	if (n != (ssize_t)sizeof(*m) || m->magic != KEMBED_MAGIC)
		return -1;

	for (struct cmsghdr *c = CMSG_FIRSTHDR(&hdr); c; c = CMSG_NXTHDR(&hdr, c))
		if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS)
			memcpy(gotfd, CMSG_DATA(c), sizeof(int));
	return 1;
}

static void send_msg(int fd, const KembedMsg *m)
{
	while (send(fd, m, sizeof(*m), MSG_NOSIGNAL) < 0 && errno == EINTR)
		;
}

int main(int argc, char **argv)
{
	const char *out = NULL;
	int w = 320, h = 200, type_key = 0;
	int i = 1;

	for (; i < argc; i++) {
		if (!strcmp(argv[i], "--size") && i + 1 < argc) {
			if (sscanf(argv[++i], "%dx%d", &w, &h) != 2)
				return 2;
		} else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
			out = argv[++i];
		} else if (!strcmp(argv[i], "--key") && i + 1 < argc) {
			type_key = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--")) {
			i++;
			break;
		} else {
			fprintf(stderr, "embedcheck: unknown option '%s'\n", argv[i]);
			return 2;
		}
	}
	if (i >= argc) {
		fprintf(stderr, "usage: embedcheck [--size WxH] [--out FILE] "
				"[--key CODE] -- CMD...\n");
		return 2;
	}

	int sv[2];

	if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) != 0) {
		perror("socketpair");
		return 1;
	}

	char size[32];

	snprintf(size, sizeof(size), "%dx%d", w, h);

	pid_t pid = fork();

	if (pid < 0) {
		perror("fork");
		return 1;
	}
	if (pid == 0) {
		const char *av[64];
		int n = 0;

		close(sv[0]);
		if (sv[1] != KEMBED_FD) {
			dup2(sv[1], KEMBED_FD);
			close(sv[1]);
		} else {
			int fl = fcntl(KEMBED_FD, F_GETFD);

			fcntl(KEMBED_FD, F_SETFD, fl & ~FD_CLOEXEC);
		}
		av[n++] = "kdos-cage";
		av[n++] = "--embed";
		av[n++] = size;
		av[n++] = "--";
		for (int j = i; j < argc && n < 62; j++)
			av[n++] = argv[j];
		av[n] = NULL;
		execvp(av[0], (char *const *)av);
		_exit(127);
	}
	close(sv[1]);

	void *map = NULL;
	size_t map_len = 0, slot_len = 0, stride = 0;
	int bw = 0, bh = 0;
	int frames = 0, rc = 1;
	uint32_t *before = NULL;	/* the frame the key was typed into */
	int typed = 0;
	int64_t deadline = now_ms() + 25000;

	while (now_ms() < deadline) {
		KembedMsg m;
		int fd = -1;
		int r = recv_msg(sv[0], &m, &fd, 500);

		if (r < 0)
			break;
		if (r == 0)
			continue;

		if (m.op == KEMBED_BUF && fd >= 0) {
			if (map)
				munmap(map, map_len);
			bw = m.a;
			bh = m.b;
			stride = (size_t)m.c;
			slot_len = (size_t)m.d;
			map_len = slot_len * KEMBED_SLOTS;
			map = mmap(NULL, map_len, PROT_READ, MAP_SHARED, fd, 0);
			close(fd);
			if (map == MAP_FAILED) {
				perror("mmap");
				map = NULL;
				break;
			}
			fprintf(stderr, "embedcheck: buffer %dx%d stride %zu\n",
				bw, bh, stride);
			continue;
		}
		if (fd >= 0)
			close(fd);

		if (m.op != KEMBED_FRAME || !map)
			continue;

		frames++;

		const uint8_t *px = (const uint8_t *)map +
				    (size_t)m.a * slot_len;
		uint32_t first = *(const uint32_t *)px;
		unsigned long ink = 0;

		/*
		 * NOT "IS IT BLACK" — the compositor paints a background, so a
		 * frame with nothing rendered into it is a uniform colour that
		 * is not black either. What proves a guest drew is that the
		 * frame has more than one colour in it.
		 *
		 * A guest has to connect, be configured and render, which takes
		 * more than the first frame — so this waits for a frame with
		 * something in it rather than for a frame count, and the
		 * deadline is what fails a guest that never drew.
		 */
		for (int y = 0; y < bh; y++)
			for (int x = 0; x < bw; x++) {
				const uint8_t *p = px + (size_t)y * stride + (size_t)x * 4;

				if (*(const uint32_t *)p != first)
					ink++;
			}

		if (ink == 0)
			continue;

		/*
		 * THE KEY IS TYPED INTO A FRAME THAT WAS ALREADY DRAWN, and the
		 * assertion is that a LATER frame differs from it. "The guest
		 * changed" is the only thing a parent holding pixels can
		 * actually observe about input having arrived, and it is enough:
		 * nothing else moves in a still terminal.
		 */
		if (type_key && !typed) {
			before = malloc((size_t)bw * (size_t)bh * 4);
			if (!before)
				break;
			for (int y = 0; y < bh; y++)
				memcpy(before + (size_t)y * bw,
				       px + (size_t)y * stride,
				       (size_t)bw * 4);

			KembedMsg k = { .magic = KEMBED_MAGIC, .op = KEMBED_KEY,
					.a = type_key, .b = 1 };

			send_msg(sv[0], &k);
			k.b = 0;
			send_msg(sv[0], &k);
			typed = 1;
			printf("typed keycode %d into a frame with %lu drawn "
			       "pixels\n", type_key, ink);
			continue;
		}

		if (type_key) {
			unsigned long moved = 0;

			for (int y = 0; y < bh; y++)
				for (int x = 0; x < bw; x++) {
					const uint32_t *a = (const uint32_t *)
						(px + (size_t)y * stride + (size_t)x * 4);

					if (*a != before[(size_t)y * bw + x])
						moved++;
				}
			if (moved == 0)
				continue;
			printf("frame %d: %lu pixels changed after the key\n",
			       frames, moved);
		} else {
			printf("frame %d: %dx%d stride %zu, %lu of %d pixels "
			       "differ from the background\n", frames, bw, bh,
			       stride, ink, bw * bh);
		}

		if (out) {
			FILE *f = fopen(out, "wb");

			if (f) {
				fprintf(f, "P6\n%d %d\n255\n", bw, bh);
				for (int y = 0; y < bh; y++)
					for (int x = 0; x < bw; x++) {
						const uint8_t *p = px + (size_t)y * stride +
								   (size_t)x * 4;
						/* The buffer is ARGB little-endian:
						 * byte 0 is blue. */
						fputc(p[2], f);
						fputc(p[1], f);
						fputc(p[0], f);
					}
				fclose(f);
			}
		}
		rc = 0;
		break;
	}

	KembedMsg bye = { .magic = KEMBED_MAGIC, .op = KEMBED_CLOSE };

	send_msg(sv[0], &bye);
	close(sv[0]);
	if (map)
		munmap(map, map_len);
	free(before);

	for (int spin = 0; spin < 200; spin++) {
		if (waitpid(pid, NULL, WNOHANG) == pid)
			break;
		usleep(20000);
	}
	kill(pid, SIGKILL);
	waitpid(pid, NULL, 0);

	if (rc != 0)
		fprintf(stderr, "embedcheck: %s\n",
			type_key ? "nothing changed after the key"
				 : "no frame with a guest drawn in it");
	return rc;
}
