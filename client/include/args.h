#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>

#define MAX_USERS 600

struct user {
    char *name;
    char *pass;
};

struct tracker_args {
    char *          log_dir;

    char *          log_file;

    char           *tracker_socks_addr;
    unsigned short  trackerSocksPort;

    char           *peer_socks_addr;
    unsigned short  peerSocksPort;

    char *          mng_addr;
    unsigned short  mng_port;

    bool            disectors_enabled;

    struct user    users[MAX_USERS];
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void parse_args(const int argc, char **argv, struct tracker_args *args);

#endif