#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/resource.h>
#include <ulimit.h>

extern char *optarg;
extern int optind, opterr, optopt;

extern char **environ;

static int is_unlimited(rlim_t v) {
    if (v == RLIM_INFINITY) return 1;
#ifdef RLIM_SAVED_MAX
    if (v == RLIM_SAVED_MAX) return 1;
#endif
#ifdef RLIM_SAVED_CUR
    if (v == RLIM_SAVED_CUR) return 1;
#endif
    return 0;
}

int main(int argc, char *argv[])
{
    const char *optstring = "ispuU:cC:dvV:";

    int flag;

    while((flag = getopt(argc, argv, optstring)) != -1)
    {
        switch(flag)
        {
            case 'i':
                printf("UID - %d\teUID - %d\tGID - %d\teGID - %d\n",
                       getuid(), geteuid(), getgid(), getegid());
                break;

            case 's':
                setpgrp();
                break;

            case 'p':
                printf("PID - %d\tPGID - %d\tPPID - %d\n",
                       getpid(), getpgrp(), getppid());
                break;

            case 'u':
#if defined(__sun) || defined(__sunos)
                {
                    long max = sysconf(_SC_CHILD_MAX);
                    if (max != -1)
                        printf("ULIMIT - %ld\n", max);
                    else
                        printf("ULIMIT - unlimited\n");
                }
#else
                {
                    struct rlimit tmp;
                    if (getrlimit(RLIMIT_NPROC, &tmp) == 0)
                        printf("ULIMIT - %ld\n", (long)tmp.rlim_cur);
                    else
                        perror("getrlimit(RLIMIT_NPROC)");
                }
#endif
                break;

            case 'U':
                {
                    long new_limit = atol(optarg);

#if defined(__sun) || defined(__sunos)
                    char cmd[256];
                    snprintf(cmd, sizeof(cmd),
                             "prctl -n task.max-processes -v %ld %d >/dev/null 2>&1",
                             new_limit, (int)getpid());
                    if (system(cmd) != 0)
                        fprintf(stderr, "set_ulimit: prctl failed\n");
#else
                    struct rlimit tmp;
                    if (getrlimit(RLIMIT_NPROC, &tmp) == 0) {
                        tmp.rlim_cur = (rlim_t)new_limit;
                        if (setrlimit(RLIMIT_NPROC, &tmp) != 0)
                            perror("setrlimit(RLIMIT_NPROC)");
                    }
#endif
                }
                break;

            case 'c':
                {
                    struct rlimit tmp;
                    if (getrlimit(RLIMIT_CORE, &tmp) == 0) {
                        printf("CORE - ");
                        if (is_unlimited(tmp.rlim_cur))
                            printf("unlimited\n");
                        else
                            printf("%llu\n",
                                   (unsigned long long)tmp.rlim_cur);
                    } else {
                        perror("getrlimit(RLIMIT_CORE)");
                    }
                }
                break;

            case 'C':
                {
                    long new_size = atol(optarg);
                    struct rlimit tmp;
                    if (getrlimit(RLIMIT_CORE, &tmp) != 0) {
                        perror("getrlimit(RLIMIT_CORE)");
                        break;
                    }
                    tmp.rlim_cur = (rlim_t)new_size;
                    if (setrlimit(RLIMIT_CORE, &tmp) != 0)
                        perror("setrlimit(RLIMIT_CORE)");
                }
                break;

            case 'd':
                printf("DIR - %s\n", getcwd(NULL, 0));
                break;

            case 'v':
                {
                    int cur = 0;
                    while(environ[cur] != NULL)
                        printf("%s\n", environ[cur++]);
                }
                break;

            case 'V':
                putenv(optarg);
                break;

            case '?':
                printf("Неизвестный аргумент - -%c\n", optopt);
                break;
        }
    }

    for(int i = optind; i < argc; i++)
        printf("Неподдерживаемый аргумент - %s\n", argv[i]);

    return 0;
}
