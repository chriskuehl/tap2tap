#ifndef TAP2TAP_H
#define TAP2TAP_H

#include <netinet/in.h>
#include <stdint.h>

#define MTU 1500

struct args {
    char *iface;
    char *remote;
    char *up_script;
    char *down_script;
    int uid;
    int gid;
};

char exit_wanted;
int received_signal;

int run_tunnel(struct args *args, sigset_t *orig_mask);
#endif
