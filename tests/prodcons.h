#pragma once

#define SOCKNAME "/tmp/mysock"

/* http://keithp.com/blogs/fd-passing/ */
ssize_t sock_fd_write(int sock, void *buf, ssize_t buflen, int fd)
{
	struct msghdr   msg;
	struct iovec	iov;
	union {
		struct cmsghdr  cmsghdr;
		char		control[CMSG_SPACE(sizeof (int))];
	} cmsgu;
	struct cmsghdr  *cmsg;

	iov.iov_base = buf;
	iov.iov_len = buflen;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	if (fd != -1) {
		msg.msg_control = cmsgu.control;
		msg.msg_controllen = sizeof(cmsgu.control);

		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof (int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;

		int *pfd = (int *) CMSG_DATA(cmsg);
		*pfd = fd;
	} else {
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
	}

	return sendmsg(sock, &msg, 0);
}

/* http://keithp.com/blogs/fd-passing/ */
ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
{
	ssize_t	 size;

	if (fd) {
		struct msghdr   msg;
		struct iovec	iov;
		union {
			struct cmsghdr  cmsghdr;
			char		control[CMSG_SPACE(sizeof (int))];
		} cmsgu;
		struct cmsghdr  *cmsg;

		iov.iov_base = buf;
		iov.iov_len = bufsize;

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = cmsgu.control;
		msg.msg_controllen = sizeof(cmsgu.control);
		size = recvmsg (sock, &msg, 0);
		if (size < 0) {
			perror ("recvmsg");
			exit(1);
		}
		cmsg = CMSG_FIRSTHDR(&msg);
		if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
			if (cmsg->cmsg_level != SOL_SOCKET) {
				fprintf (stderr, "invalid cmsg_level %d\n",
					 cmsg->cmsg_level);
				exit(1);
			}
			if (cmsg->cmsg_type != SCM_RIGHTS) {
				fprintf (stderr, "invalid cmsg_type %d\n",
					 cmsg->cmsg_type);
				exit(1);
			}

			int *pfd = (int *) CMSG_DATA(cmsg);
			*fd = *pfd;
		} else
			*fd = -1;
	} else {
		size = read (sock, buf, bufsize);
		if (size < 0) {
			perror("read");
			exit(1);
		}
	}
	return size;
}

enum class MsgType : uint8_t
{
	SetCrtc = 1,
};

struct MsgFramebuffer
{
	int width;
	int height;
	kms::PixelFormat format;

	int num_planes;
};

struct MsgPlane
{
	int pitch;
	int offset;
};


struct MsgSetCrtc
{
	uint32_t connector;
	uint32_t crtc;

	kms::Videomode mode;
};

