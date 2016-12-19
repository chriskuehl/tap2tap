/*
 * simple point-to-point L2 (tap) vpn
 *
 * it doesn't do anything fancy: no IP assignment, no crazy CA system, no
 * support for legacy systems. it runs only on modern linux. it is L2 only.
 *
 * features still needed, in order of priority:
 *   - basic command-line interface
 *   - ability to call a script after the interface is ready (to configure IPs, etc)
 *   - tests
 *   - encryption
 *   - privilege-dropping at runtime
 *
 * https://github.com/chriskuehl/tap2tap
 */
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <net/if.h>
#include <linux/if.h>
#include <linux/if_tun.h>


const char *clone = "/dev/net/tun";
const char *device = "tap1";


int up_iface(void) {
    struct ifreq req;
    memset(&req, 0, sizeof req);
    req.ifr_flags = IFF_UP;

    if (strlen(device) + 1 >= IFNAMSIZ) {
        fprintf(stderr, "device name is too long: %s\n", device);
        return -1;
    }
    strncpy(req.ifr_name, device, IFNAMSIZ);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return sockfd;
    }

    int err = ioctl(sockfd, SIOCSIFFLAGS, &req);
    if (err < 0) {
        perror("ioctl");
        close(sockfd);
        return err;
    }

    close(sockfd);
    return 0;
}


int setup_tap(void) {
    // https://raw.githubusercontent.com/torvalds/linux/master/Documentation/networking/tuntap.txt
    int fd = open(clone, O_RDWR);
    if (fd < 0) {
        perror("open");
        return fd;
    }

    struct ifreq req;
    memset(&req, 0, sizeof req);
    req.ifr_flags = IFF_TAP;

    if (strlen(device) + 1 >= IFNAMSIZ) {
        close(fd);
        fprintf(stderr, "device name is too long: %s\n", device);
        return -1;
    }
    strncpy(req.ifr_name, device, IFNAMSIZ);

    int err = ioctl(fd, TUNSETIFF, &req);
    if (err < 0) {
        close(fd);
        perror("ioctl");
        return err;
    }

    err = up_iface();
    if (err < 0) {
        close(fd);
        return err;
    }

    return fd;
}


int setup_socket(in_addr_t bind_addr, uint16_t bind_port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("socket");
        return sockfd;
    }

    struct sockaddr_in local;
    memset(&local, 0, sizeof local);
    local.sin_family = AF_INET;
    local.sin_port = htons(bind_port);
    local.sin_addr.s_addr = bind_addr;

    int bind_result = bind(sockfd, (struct sockaddr *) &local, sizeof local);
    if (bind_result < 0) {
        perror("bind");
        return bind_result;
    }

    return sockfd;
}


int main(void) {
    int fd = setup_tap();
    if (fd < 0) {
        fprintf(stderr, "unable to create tap device\n");
        return 1;
    }

    int sockfd = setup_socket(inet_addr("0.0.0.0"), 1234);
    if (sockfd < 0) {
        fprintf(stderr, "unable to create socket\n");
        return 2;
    }

    // TODO: buffers are really crummy data structures and really don't work
    // here since each write to the tap device must be one entire frame; the
    // only reason this works is luck (and lots of retries)
    //
    // ultimately we probably need a queue of frames here

    // bytes received from remote, want to write to fd
    char received_buffer[8192];
    size_t received_len = 0;

    // bytes read from fd, want to write to remote
    char send_buffer[8192];
    size_t send_len = 0;

    struct timespec tm;
    memset(&tm, 0, sizeof tm);
    tm.tv_nsec = 10000000;  // 0.01 seconds

    struct pollfd fds[2];
    memset(&fds, 0, sizeof fds);
    fds[0].fd = fd;
    fds[1].fd = sockfd;

    struct sockaddr_in remote;
    memset(&remote, 0, sizeof remote);
    char has_remote = 0;

    // TODO: this should come from the command line
    char *server = getenv("SERVER");
    if (server) {
        remote.sin_family = AF_INET;
        // TODO: error handling
        remote.sin_addr.s_addr = inet_addr(server);
        remote.sin_port = htons(1234);
        has_remote = 1;
    }

    for (;;) {
        fds[0].events = POLLIN;
        if (received_len > 0) {
            fds[0].events |= POLLOUT;
        }

        fds[1].events = POLLIN;
        if (send_len > 0 && has_remote) {
            fds[1].events |= POLLOUT;
        }

        // TODO: handle signals properly
        int result = ppoll(fds, 2, &tm, NULL);
        if (result < 0) {
            perror("ppoll");
            return 3;
        }

        // tap can handle a write
        if (fds[0].revents & POLLOUT) {
            // TODO: nonblocking?
            ssize_t n = write(fd, received_buffer, received_len);
            if (n < 0) {
                if (errno == EINVAL) {
                    fprintf(stderr, "wrote garbage frame\n");
                    // this entire buffer is bunk, dunno how far to skip
                    received_len = 0;
                } else {
                    perror("write");
                    return 4;
                }
            } else {
                printf("wrote %zd bytes to tap\n", n);
                received_len -= n;
                if (received_len > 0) {
                    memmove(received_buffer, received_buffer + n, received_len);
                }
            }
        }

        // udp socket can handle a write
        if (fds[1].revents & POLLOUT) {
            // TODO: nonblocking?
            printf("want to write up to: %zu\n", received_len);
            ssize_t n = sendto(sockfd, send_buffer, send_len, 0, (struct sockaddr *) &remote, sizeof remote);
            if (n < 0) {
                perror("write");
                return 4;
            } else {
                printf("wrote %zd bytes\n", n);
                send_len -= n;
                if (send_len > 0) {
                    memmove(send_buffer, send_buffer + n, send_len);
                }
            }
        }

        // tap has data for us to read
        if (fds[0].revents & POLLIN && send_len < sizeof(send_buffer)) {
            ssize_t n = read(fd, send_buffer, sizeof(send_buffer) - send_len);
            printf("read from tap: %zd\n", n);
            if (n < 0) {
                perror("read");
                return 4;
            } else {
                send_len += n;
            }
        }


        // udp socket has data for us to read
        if (fds[1].revents & POLLIN && received_len < sizeof(received_buffer)) {
            socklen_t l = sizeof remote;
            has_remote = 1;
            ssize_t n = recvfrom(
                sockfd,
                received_buffer,
                sizeof(received_buffer) - received_len,
                0,
                (struct sockaddr *) &remote,
                &l
            );
            printf("read from udp: %zd\n", n);
            if (n < 0) {
                perror("recvfrom");
                return 4;
            }
            received_len += n;
        }
    }
}
