#include <assert.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tap2tap.h"


const char *VERSION = "0.0.0";
const int QUIT_SIGNALS[] = {SIGTERM, SIGINT, SIGHUP, SIGQUIT};

void handle_signal(int signum) {
    exit_wanted = 1;
    received_signal = signum;
}


void print_help(int argc, char *argv[]) {
    assert(argc >= 1);
    fprintf(stderr, "tap2tap v%s\n", VERSION);
    fprintf(stderr, "Usage: %s [--dev {device}] [--remote {remote}]\n", argv[0]);
    fprintf(stderr, "       %*s [--up {binary}] [--down {binary}]\n", (unsigned int) strlen(argv[0]), "");
    fprintf(stderr, "\n");
    fprintf(stderr, "Optional arguments:\n");
    fprintf(stderr, "  -i, --iface {iface}  Name of the tap device interface.\n");
    fprintf(stderr, "                       (default: kernel auto-assign)\n");
    fprintf(stderr, "      --remote {addr}  IPv4 address of the remote peer.\n");
    fprintf(stderr, "      --up {binary}    Binary to excecute when the interface is up.\n");
    fprintf(stderr, "                       The only argument passed will be the interface name.\n");
    fprintf(stderr, "      --down {binary}  Binary to execute after the tunnel closes.\n");
    fprintf(stderr, "                       The only argument passed will be the interface name.\n");
    fprintf(stderr, "                       At this point, the interface still exists.\n");
    fprintf(stderr, "  -u, --uid {uid}      uid to drop privileges to (default: 65534)\n");
    fprintf(stderr, "  -g, --gid {gid}      gid to drop privileges to (default: 65534)\n");
    fprintf(stderr, "  -h, --help           Print this help message and exit.\n");
    fprintf(stderr, "  -V, --version        Print the current version and exit.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Example usage:\n");
    fprintf(stderr, "  On your server: %s -i tap0\n", argv[0]);
    fprintf(stderr, "  This creates a tap device and waits for UDP connections from any host.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  On your client: %s -i tap0 --remote 1.2.3.4\n", argv[0]);
    fprintf(stderr, "  This creates a tap device and tries to connect to the remote host.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "tap2tap has two basic modes: server and client. Both modes work by\n");
    fprintf(stderr, "creating a tap device and shuffling packets back-and-forth over UDP.\n");
    fprintf(stderr, "In server mode, however, no traffic is sent until a connection from a\n");
    fprintf(stderr, "client is received. In client mode, traffic is immediately sent to the\n");
    fprintf(stderr, "remote IP address (and incoming traffic is only accepted from that\n");
    fprintf(stderr, "IP).\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "If you're tunneling between two hosts with static IPs, you can specify\n");
    fprintf(stderr, "--remote on both ends of the tunnel.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "In all cases, at least one host must specify --remote.\n");
}


void cli_parse(struct args *args, int argc, char *argv[]) {
    int opt;
    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {"remote", required_argument, NULL, 'r'},
        {"iface", required_argument, NULL, 'i'},
        {"up", required_argument, NULL, 'U'},
        {"down", required_argument, NULL, 'D'},
        {"uid", required_argument, NULL, 'u'},
        {"gid", required_argument, NULL, 'g'},
        {NULL, 0, NULL, 0},
    };
    while ((opt = getopt_long(argc, argv, "+hVi:u:g:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argc, argv);
                exit(0);
            case 'V':
                fprintf(stderr, "tap2tap v%s\n", VERSION);
                exit(0);
            case 'r':
                args->remote = optarg;
                break;
            case 'i':
                args->iface = optarg;
                break;
            case 'U':
                args->up_script = optarg;
                break;
            case 'D':
                args->down_script = optarg;
                break;
            case 'u':
                args->uid = atoi(optarg);
                break;
            case 'g':
                args->gid = atoi(optarg);
                break;
            default:
                exit(1);
        }
    }
    if (argv[optind]) {
        fprintf(stderr, "error: extra argument given: %s\n", argv[optind]);
        exit(1);
    }
}


int main(int argc, char *argv[]) {
    struct args args;
    memset(&args, 0, sizeof args);
    args.uid = 65534;
    args.gid = 65534;
    cli_parse(&args, argc, argv);

    sigset_t mask;
    sigset_t orig_mask;
    sigemptyset(&mask);
    sigemptyset(&orig_mask);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = handle_signal;

    for (size_t i = 0; i < sizeof(QUIT_SIGNALS) / sizeof(int); i++) {
        int signum = QUIT_SIGNALS[i];
        sigaddset(&mask, signum);
        if (sigaction(signum, &act, 0)) {
            perror("sigaction");
            return 1;
        }
    }

    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) != 0) {
        perror("sigprocmask");
        return 1;
    }

    return run_tunnel(&args, &orig_mask);
}
