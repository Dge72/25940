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
    {
        errno = 0;
        long v = ulimit(UL_GETFSIZE);
        if (v == -1 && errno != 0)
            perror("ulimit(UL_GETFSIZE)");
        else if (v == -1)
            printf("ULIMIT - unlimited\n");
        else
            printf("ULIMIT - %ld\n", v);
    }
    break;

case 'U':
    {
        char *end;
        errno = 0;
        long new_limit = strtol(optarg, &end, 10);
        if (errno != 0 || end == optarg || *end != '\0') {
            fprintf(stderr, "invalid -U value: '%s'\n", optarg);
            break;
        }
        errno = 0;
        long r = ulimit(UL_SETFSIZE, new_limit);
        if (r == -1 && errno != 0)
            perror("ulimit(UL_SETFSIZE)");
    }
    break;
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
