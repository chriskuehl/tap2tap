/*
 * simple point-to-point L2 (tap) vpn
 *
 * it doesn't do anything fancy: no IP assignment, no crazy CA system, no
 * support for legacy systems. it runs only on modern linux. it is L2 only.
 *
 * features still needed, in order of priority:
 *   - tests
 *   - encryption
 *   - privilege-dropping at runtime
 *
 * https://github.com/chriskuehl/tap2tap
 */
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <assert.h>
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
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "tap2tap.h"
#include "iface.h"

#define RECV_QUEUE 1024
#define SEND_QUEUE 1024


char exit_wanted = 0;
int received_signal = 0;

struct frame {
    size_t len;
    char data[MTU];
};


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




int run_updown(char *script, char *device) {
    pid_t pid = fork();
    if (pid == 0) {  // child
        execlp(script, script, device, NULL);
        // exec failed
        perror("exec");
        exit(1);
    } else if (pid > 0) {  // parent
        int ret;
        waitpid(pid, &ret, 0);
        return WEXITSTATUS(ret);
    } else {
        perror("fork");
        exit(1);
    }
}



int run_tunnel(struct args *args, sigset_t *orig_mask) {
    char device[IFNAMSIZ];
    int fd = create_tap(args->iface, device, MTU);
    if (fd < 0) {
        fprintf(stderr, "unable to create tap device\n");
        return 1;
    }
    printf("tap device is: %s\n", device);

    if (args->up_script) {
        int ret = run_updown(args->up_script, device);
        if (ret != 0) {
            fprintf(stderr, "up script exited with status: %d\n", ret);
            return 1;
        }
    }

    // TODO: make the port and bind address configurable
    int sockfd = setup_socket(inet_addr("0.0.0.0"), 1234);
    if (sockfd < 0) {
        fprintf(stderr, "unable to create socket\n");
        return 2;
    }


    // circular queues
    struct frame recv_queue[RECV_QUEUE] = {0};
    size_t recv_idx = 0;
    size_t recv_len = 0;

    struct frame send_queue[SEND_QUEUE] = {0};
    size_t send_idx = 0;
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

    if (args->remote) {
        remote.sin_family = AF_INET;
        remote.sin_port = htons(1234);
        has_remote = 1;

        remote.sin_addr.s_addr = inet_addr(args->remote);
        if (remote.sin_addr.s_addr == INADDR_NONE) {
            fprintf(stderr, "failed to parse remote: %s\n", args->remote);
            return 2;
        }
        fprintf(stderr, "running in client mode with remote: %s\n", args->remote);
    }


    fprintf(stderr, "tunnel is up\n");
    for (;;) {
        fds[0].events = POLLIN;
        if (recv_len > 0) {
            fds[0].events |= POLLOUT;
        }

        fds[1].events = POLLIN;
        if (send_len > 0 && has_remote) {
            fds[1].events |= POLLOUT;
        }

        int result = ppoll(fds, 2, &tm, orig_mask);
        if (result < 0) {
            if (errno != EINTR) {
                perror("ppoll");
                return 3;
            }
        }

        if (exit_wanted) {
            fprintf(stderr, "\nreceived signal %d, stopping tunnel\n", received_signal);
            break;
        }

        // tap can handle a write
        if (fds[0].revents & POLLOUT) {
            struct frame *f = &recv_queue[recv_idx];
            assert(f->len <= MTU);
            recv_idx = (recv_idx + 1) % RECV_QUEUE;
            recv_len -= 1;

            ssize_t n = write(fd, f->data, f->len);
            if (n < 0) {
                if (errno == EINVAL) {
                    fprintf(stderr, "received garbage frame\n");
                } else {
                    perror("write");
                    return 4;
                }
            } else if (n < f->len) {
                printf("[error] only wrote %zd bytes to tap (out of %zd bytes)\n", n, f->len);
            }
        }

        // udp socket can handle a write
        if (fds[1].revents & POLLOUT) {
            struct frame *f = &send_queue[send_idx];
            assert(f->len <= MTU);
            send_idx = (send_idx + 1) % SEND_QUEUE;
            send_len -= 1;

            ssize_t n = sendto(sockfd, f->data, f->len, 0, (struct sockaddr *) &remote, sizeof remote);
            if (n < 0) {
                perror("sendto");
                return 4;
            } else if (n < f->len) {
                printf("[error] only sent %zd bytes to peer (out of %zd bytes)\n", n, f->len);
            }
        }

        // tap has data for us to read
        if (fds[0].revents & POLLIN) {
            size_t idx = (send_idx + send_len) % SEND_QUEUE;

            if (send_len < SEND_QUEUE) {
                send_len += 1;
            } else {
                assert(send_len == SEND_QUEUE);
                printf("dropping frame from send queue\n");

                // put this packet at the end of the queue;
                // drop the first frame in the queue
                send_idx += 1;
            }

            struct frame *f = &send_queue[idx];
            memset(f, 0, sizeof(struct frame));
            ssize_t n = read(fd, &f->data, MTU);
            assert(n <= MTU);
            f->len = n;
        }

        // udp socket has data for us to read
        if (fds[1].revents & POLLIN) {
            size_t idx = (recv_idx + recv_len) % RECV_QUEUE;

            if (recv_len < RECV_QUEUE) {
                recv_len += 1;
            } else {
                assert(recv_len == RECV_QUEUE);
                printf("dropping frame from recv queue\n");

                // put this packet at the end of the queue;
                // drop the first frame in the queue
                recv_idx += 1;
            }

            struct frame *f = &recv_queue[idx];
            memset(f, 0, sizeof(struct frame));

            // TODO: handle case where remote changes, in both server+client mode
            socklen_t l = sizeof(remote);
            has_remote = 1;
            ssize_t n = recvfrom(
                sockfd,
                &f->data,
                MTU,
                0,
                (struct sockaddr *) &remote,
                &l
            );
            assert(n <= MTU);
            f->len = n;
        }
    }

    if (args->down_script) {
        int ret = run_updown(args->down_script, device);
        if (ret != 0) {
            fprintf(stderr, "down script exited with status: %d\n", ret);
            return 1;
        }
    }

    return 0;
}
