#include <cstdio>
#include <algorithm>

#include <drm.h>
#include <xf86drm.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "kms++.h"

#include "test.h"
#include "prodcons.h"

using namespace std;
using namespace kms;

static int start_server()
{
	int r;

	printf("starting server\n");

	unlink(SOCKNAME);

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ASSERT(fd >= 0);

	struct sockaddr_un addr = { 0 };
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCKNAME);
	r = bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
	ASSERT(r == 0);

	r = listen(fd, 0);
	ASSERT(r == 0);

	printf("listening...\n");

	return fd;
}

static int cfd = -1;

#if 0
static void receive_fb(int sfd, int *output_id, struct framebuffer *fb)
{
	char buf[16];
	int prime_fd;
	int r;

	size_t size = sock_fd_read(sfd, buf, sizeof(buf), &prime_fd);
	ASSERT(size == 1);

	*output_id = buf[0];

	struct modeset_out *out = find_output(modeset_list, *output_id);
	ASSERT(out);

	int w = out->mode.hdisplay;
	int h = out->mode.vdisplay;

	ASSERT(w != 0 && h != 0);

	r = drmPrimeFDToHandle(global.drm_fd, prime_fd, &fb->planes[0].handle);
	ASSERT(r == 0);

	fb->fd = global.drm_fd;
	fb->num_planes = 1;

	fb->width = w;
	fb->height = h;
	fb->planes[0].stride = fb->width * 32 / 8;
	fb->planes[0].size = fb->planes[0].stride * fb->height;

	r = drmModeAddFB(global.drm_fd, fb->width, fb->height, 24, 32, fb->planes[0].stride,
		   fb->planes[0].handle, &fb->fb_id);
	ASSERT(r == 0);

	//printf("received fb handle %x, prime %d, fb %d\n", fb->planes[0].handle, prime_fd, fb->fb_id);

	r = close(prime_fd);
	ASSERT(r == 0);
}

#endif



static void receive_msg(Card& card, MsgType msg_type)
{
	int r;

	switch (msg_type) {
	case MsgType::SetCrtc:
	{
		int fd;
		size_t len;

		MsgSetCrtc crtcmsg;
		len = sock_fd_read(cfd, &crtcmsg, sizeof(crtcmsg), &fd);
		ASSERT(len == sizeof(crtcmsg));
		printf("recv set-crtc, fd %d\n", fd);

		MsgFramebuffer fbmsg;
		len = sock_fd_read(cfd, &fbmsg, sizeof(fbmsg), &fd);
		ASSERT(len == sizeof(fbmsg));
		printf("recv fb, fd %d\n", fd);

		uint32_t handles[4];
		uint32_t pitches[4];
		uint32_t offsets[4];
		for (int i = 0; i < fbmsg.num_planes; ++i) {
			MsgPlane planemsg;
			len = sock_fd_read(cfd, &planemsg, sizeof(planemsg), &fd);
			ASSERT(len == sizeof(planemsg));
			printf("recv plane, fd %d\n", fd);

			uint32_t handle;
			r = drmPrimeFDToHandle(card.fd(), fd, &handle);
			ASSERT(r == 0);

			handles[i] = handle;
			pitches[i] = planemsg.pitch;
			offsets[i] = planemsg.offset;
		}

		auto fb = new ExtFramebuffer(card, fbmsg.width, fbmsg.height, fbmsg.format,
					     handles, pitches, offsets);

		auto conn = card.get_connector(crtcmsg.connector);
		auto crtc = card.get_crtc(crtcmsg.crtc);
		auto mode = crtcmsg.mode;

		int r = crtc->set_mode(conn, *fb, mode);
		ASSERT(r == 0);

		break;
	}

	}
}

static void main_loop(Card& card, int sfd)
{
	fd_set fds;

	FD_ZERO(&fds);

	int fd = card.fd();

	printf("press enter to exit\n");

	while (true) {
		int r;

		FD_SET(0, &fds);
		FD_SET(fd, &fds);
		FD_SET(sfd, &fds);

		int max_fd = max(fd, cfd);

		if (cfd >= 0) {
			FD_SET(cfd, &fds);
			max_fd = max(max_fd, cfd);
		}

		r = select(max_fd + 1, &fds, NULL, NULL, NULL);
		if (r < 0) {
			fprintf(stderr, "select() failed with %d: %m\n", errno);
			return;
		}

		if (FD_ISSET(0, &fds)) {
			fprintf(stderr, "exit due to user-input\n");
			return;
		}

		if (FD_ISSET(fd, &fds))
			card.call_page_flip_handlers();

		if (FD_ISSET(sfd, &fds)) {
			printf("server socket event\n");

			int new_fd = accept(sfd, NULL, NULL);
			ASSERT(new_fd > 0);
			if (cfd != -1)
				close(new_fd);
			else
				cfd = new_fd;
		}

		if (cfd >= 0 && FD_ISSET(cfd, &fds)) {
			printf("in socket event\n");

			MsgType id;
			size_t len = sock_fd_read(cfd, &id, 1, 0);

			printf("recv %zd, id %d\n", len, id);

			if (len == 0) {
				printf("client disconnect\n");
				close(cfd);
				cfd = -1;
			} else {
				receive_msg(card, id);


			}
		}
	}
}


int main()
{
	int sfd = start_server();

	printf("accepted connection\n");


	Card card;


	main_loop(card, sfd);




	close(cfd);
	close(sfd);
	unlink(SOCKNAME);

	printf("exit\n");
}
