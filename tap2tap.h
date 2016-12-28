#ifndef TAP2TAP_H
#define TAP2TAP_H

#include <netinet/in.h>
#include <stdint.h>

#define MAX_MTU 1500
#define ETHERNET_HEADER 14

struct args {
    char *iface;
    char *remote;
    char *up_script;
    char *down_script;
    int uid;
    int gid;
    unsigned int mtu;
};

char exit_wanted;
int received_signal;

int run_tunnel(struct args *args, sigset_t *orig_mask);
#endif
