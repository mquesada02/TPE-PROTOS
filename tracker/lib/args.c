#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>

#include "../include/args.h"

static unsigned short port(const char *s) {
     char *end     = 0;
     const long sl = strtol(s, &end, 10);

     if (end == s || '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) 
        || sl < 0 || sl > USHRT_MAX) {
         fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
         exit(1);
         return 1;
     }
     return (unsigned short)sl;
}

static void user(char *s, struct user *user) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found\n");
        exit(1);
    } else {
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
    }

}

static void version(void) {
    fprintf(stderr, "Tracker version 0.1\n"
                    "ITBA Protocolos de Comunicación 2024/1 -- Grupo 3\n"
                    "MIT License\n"
                    "Copyright (c) 2024 mquesada tm-sm catamuller NCasella\n"
                    "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
                    "of this software and associated documentation files (the \"Software\"), to deal\n"
                    "in the Software without restriction, including without limitation the rights\n"
                    "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
                    "copies of the Software, and to permit persons to whom the Software is\n"
                    "furnished to do so, subject to the following conditions:\n"

                    "The above copyright notice and this permission notice shall be included in all\n"
                    "copies or substantial portions of the Software.\n"

                    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
                    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
                    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
                    "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
                    "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
                    "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
                    "SOFTWARE.\n");
}

static void usage(const char *progname) {
    fprintf(stderr,
        "Usage: %s [OPTION]...\n"
        "\n"
        "   -h               Imprime la ayuda y termina.\n"
        "   -l               Habilita el flag de localhost\n"
        "   -P <con port>    Puerto entrante para conexiones.\n"
        "\n",
        progname);
    exit(1);
}

void parse_args(const int argc, char **argv, struct tracker_args *args) {
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    args->socks_addr = "0.0.0.0";
    args->socks_port = 15555;

    args->mng_addr   = "127.0.0.1";
    args->mng_port   = 8080;

    args->isLocalhost = false;

    //args->conf_addr  = "127.0.0.1";
    //args->conf_port  = 2526;

    args->disectors_enabled = true;

    int c;

    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            { 0,           0,                 0, 0 }
        };

        c = getopt_long(argc, argv, "hlP:v", long_options, &option_index);

        if (c == -1)
            break;
        switch (c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'l':
                args->isLocalhost = true;
                break;
            case 'P':
                args->socks_port = port(optarg);
                break;
            case 'v':
                version();
                exit(0);
                break;
            default:
                fprintf(stderr, "unknown argument %d.\n", c);
                exit(1);
        }

    }
    if (optind < argc) {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }
}
