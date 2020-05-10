// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cstdio>
#include <cstdlib>

#include <signal.h>
#include <argtable2.h>

const int MAX_LISTEN_ADDR = 256;

int multi_proxy_main(const char* path, const char** lis_addrs, int lis_count, const char* fwd_addr, bool use_pp);

int main(int argc, char **argv)
{
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, SIG_IGN);
#endif

    const char* progname = "puicproxy";
    struct arg_str*  listen  = arg_strn("l", "listen", "<ip:port>", 1, MAX_LISTEN_ADDR, "net addresses to listen on");
    struct arg_str*  forward = arg_str1("f", "forward", "<ip:port>", "net addresses to forward to");
    struct arg_lit*  use_pp  = arg_lit0("u", "proxy_proto", "use the PROXY protocol to forward");
    struct arg_lit*  help    = arg_lit0("h", "help", "print this help and exit");
    struct arg_end*  end     = arg_end(20);
    void* argtable[] = {listen, forward, use_pp, help, end};
    int exitcode = 0, nerrors = 0;

    if (arg_nullcheck(argtable) != 0)
    {
        /* NULL entries were detected, some allocations must have failed */
        printf("%s: insufficient memory\n", progname);
        exitcode = 1;
        goto exit;
    }

    nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        printf("Usage: %s", progname);
        arg_print_syntax(stdout, argtable,"\n");
        printf("Proxy for puic protocol.\n\n");
        arg_print_glossary(stdout, argtable,"  %-25s %s\n");
        printf("\n");
        exitcode = 0;
        goto exit;
    }

    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more information.\n", progname);
        exitcode = 1;
        goto exit;
    }

    exitcode = multi_proxy_main(argv[0], listen->sval, listen->count, forward->sval[0], use_pp->count > 0);

exit:
    /* deallocate each non-null entry in argtable[] */
    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));

    return exitcode;
}
