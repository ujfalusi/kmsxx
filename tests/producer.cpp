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


static void send_fb(Card& card, int cfd, int output_id, DumbFramebuffer& fb)
{
	int prime_fd;
	int r;
	char buf[1];

	r = drmPrimeHandleToFD(card.fd(), fb.handle(0), DRM_CLOEXEC, &prime_fd);
	ASSERT(r == 0);

	buf[0] = output_id;

	size_t size = sock_fd_write(cfd, buf, 1, prime_fd);
	ASSERT(size == 1);

	//printf ("sent fb handle %x, output %d, prime %d\n", fb->handle, output_id, prime_fd);

	r = close(prime_fd);
	ASSERT(r == 0);
}

static int connect_to_server()
{
	printf("connecting\n");

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ASSERT(fd != 0);

	struct sockaddr_un addr = { 0 };
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCKNAME);

	int r = connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
	ASSERT(r == 0);

	printf("connected, fd %d\n", fd);

	return fd;
}

static void main_loop(Card& card, int cfd)
{
	fd_set fds;

	FD_ZERO(&fds);

	int fd = card.fd();

	printf("press enter to exit\n");

	while (true) {
		int r;

		FD_SET(0, &fds);
		FD_SET(fd, &fds);
		FD_SET(cfd, &fds);

		int max_fd = max(fd, cfd);

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

		if (FD_ISSET(cfd, &fds)) {
			printf("socket event\n");
		}
	}
}

void send_fb()
{
	{
		MsgFramebuffer msg;
		msg.format = fb->format();
		msg.width = fb->width();
		msg.height = fb->height();
		msg.num_planes = fb->num_planes();
		len = sock_fd_write(cfd, &msg, sizeof(msg), -1);
		ASSERT(len == sizeof(msg));
	}

	for (unsigned i = 0; i < fb->num_planes(); ++i) {
		MsgPlane msg;
		msg.pitch = fb->stride(i);
		msg.offset = 0;

		int fd;
		//int r = drmPrimeHandleToFD(card.fd(), fb->handle(i), DRM_CLOEXEC, &fd);
		//ASSERT(r == 0);
		fd = 1;

		len = sock_fd_write(cfd, &msg, sizeof(msg), fd);
		ASSERT(len == sizeof(msg));
	}



}

void send_set_crtc(Connector* conn, Crtc* crtc, const Videomode& mode, Framebuffer* fb)
{
	size_t len;

	MsgType id = MsgType::SetCrtc;
	len = sock_fd_write(cfd, &id, 1, -1);
	ASSERT(len == 1);

	MsgSetCrtc msg;
	msg.connector = conn->id();
	msg.crtc = crtc->id();
	msg.mode = mode;
	len = sock_fd_write(cfd, &msg, sizeof(msg), -1);
	ASSERT(len == sizeof(msg));

}

int main()
{
	int cfd = connect_to_server();

	Card card;
	drmDropMaster(card.fd());

	auto conn = card.get_first_connected_connector();
	auto crtc = conn->get_current_crtc();

	vector<Framebuffer*> fbs;

	auto fb = new DumbFramebuffer(card, 400, 400, PixelFormat::XRGB8888);
	draw_test_pattern(*fb);
	fbs.push_back(fb);



	//main_loop(card, cfd);


	for(auto fb : fbs)
		delete fb;
}
