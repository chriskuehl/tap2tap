#define _GNU_SOURCE

#include <fcntl.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/if.h>
#include <linux/if_tun.h>


int up_iface(char *name) {
    struct ifreq req;
    memset(&req, 0, sizeof req);
    req.ifr_flags = IFF_UP;

    if (strlen(name) + 1 >= IFNAMSIZ) {
        fprintf(stderr, "device name is too long: %s\n", name);
        return -1;
    }
    strncpy(req.ifr_name, name, IFNAMSIZ);

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


int set_mtu(char *name, unsigned int mtu) {
    struct ifreq req;
    memset(&req, 0, sizeof req);
    req.ifr_mtu = mtu;

    if (strlen(name) + 1 >= IFNAMSIZ) {
        fprintf(stderr, "device name is too long: %s\n", name);
        return -1;
    }
    strncpy(req.ifr_name, name, IFNAMSIZ);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return sockfd;
    }

    int err = ioctl(sockfd, SIOCSIFMTU, &req);
    if (err < 0) {
        perror("ioctl");
        close(sockfd);
        return err;
    }

    close(sockfd);
    return 0;
}


int create_tap(char *name, char return_name[IFNAMSIZ], unsigned int mtu) {
    // https://raw.githubusercontent.com/torvalds/linux/master/Documentation/networking/tuntap.txt
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        perror("open");
        return fd;
    }

    struct ifreq req;
    memset(&req, 0, sizeof req);
    req.ifr_flags = IFF_TAP | IFF_NO_PI;

    if (name) {
        if (strlen(name) + 1 >= IFNAMSIZ) {
            close(fd);
            fprintf(stderr, "device name is too long: %s\n", name);
            return -1;
        }
        strncpy(req.ifr_name, name, IFNAMSIZ);
    }

    int err = ioctl(fd, TUNSETIFF, &req);
    if (err < 0) {
        close(fd);
        perror("ioctl");
        return err;
    }

    strncpy(return_name, req.ifr_name, IFNAMSIZ);
    return_name[IFNAMSIZ - 1] = '\0';

    // TODO: why must subtract here?
    err = set_mtu(return_name, mtu - 50);
    if (err < 0) {
        close(fd);
        return err;
    }

    err = up_iface(return_name);
    if (err < 0) {
        close(fd);
        return err;
    }

    return fd;
}
