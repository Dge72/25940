#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

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

static void print_rlim_value(rlim_t v) {
    if (is_unlimited(v))
        printf("unlimited");
    else
        printf("%llu", (unsigned long long)v);
}

void print_ids(void) {
    uid_t ruid = getuid(), euid = geteuid();
    gid_t rgid = getgid(), egid = getegid();
    struct passwd *pw;
    struct group *gr;

    printf("real uid=%d, effective uid=%d\n", (int)ruid, (int)euid);
    printf("real gid=%d, effective gid=%d\n", (int)rgid, (int)egid);

    pw = getpwuid(ruid);
    if (pw) printf("real user=%s\n", pw->pw_name);
    pw = getpwuid(euid);
    if (pw) printf("effective user=%s\n", pw->pw_name);

    gr = getgrgid(rgid);
    if (gr) printf("real group=%s\n", gr->gr_name);
    gr = getgrgid(egid);
    if (gr) printf("effective group=%s\n", gr->gr_name);
}

void print_pids(void) {
    printf("pid=%d\n", (int)getpid());
    printf("ppid=%d\n", (int)getppid());
    printf("pgid=%d\n", (int)getpgrp());
}

void print_ulimit(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_FSIZE, &rl) == 0) {
        printf("ulimit (RLIMIT_FSIZE): soft=");
        print_rlim_value(rl.rlim_cur);
        printf(", hard=");
        print_rlim_value(rl.rlim_max);
        printf("\n");
    } else {
        perror("getrlimit(RLIMIT_FSIZE)");
    }
}

void set_ulimit(const char *val) {
    char *end;
    long v;
    struct rlimit rl;

    errno = 0;
    v = strtol(val, &end, 10);
    if (errno != 0 || end == val || *end != '\0' || v < 0) {
        fprintf(stderr, "invalid ulimit value: '%s'\n", val);
        return;
    }

    if (getrlimit(RLIMIT_FSIZE, &rl) != 0) {
        perror("getrlimit(RLIMIT_FSIZE)");
        return;
    }
    rl.rlim_cur = (rlim_t)v;
    if (setrlimit(RLIMIT_FSIZE, &rl) != 0)
        perror("setrlimit(RLIMIT_FSIZE)");
}

void print_core(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == 0) {
        printf("core size: soft=");
        print_rlim_value(rl.rlim_cur);
        printf(", hard=");
        print_rlim_value(rl.rlim_max);
        printf("\n");
    } else {
        perror("getrlimit(RLIMIT_CORE)");
    }
}

void set_core(const char *val) {
    char *end;
    long v;
    struct rlimit rl;

    errno = 0;
    v = strtol(val, &end, 10);
    if (errno != 0 || end == val || *end != '\0' || v < 0) {
        fprintf(stderr, "invalid core size: '%s'\n", val);
        return;
    }

    if (getrlimit(RLIMIT_CORE, &rl) != 0) {
        perror("getrlimit(RLIMIT_CORE)");
        return;
    }
    rl.rlim_cur = (rlim_t)v;
    if (setrlimit(RLIMIT_CORE, &rl) != 0)
        perror("setrlimit(RLIMIT_CORE)");
}

void print_cwd(void) {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) != NULL)
        printf("cwd=%s\n", buf);
    else
        perror("getcwd");
}

void print_env(void) {
    for (char **e = environ; *e != NULL; e++)
        printf("%s\n", *e);
}

void set_env(const char *arg) {
    char *eq = strchr(arg, '=');
    if (eq == NULL || eq == arg) {
        fprintf(stderr, "invalid -V format (expected name=value): '%s'\n", arg);
        return;
    }

    size_t namelen = (size_t)(eq - arg);
    char *name = malloc(namelen + 1);
    if (name == NULL) {
        perror("malloc");
        return;
    }
    memcpy(name, arg, namelen);
    name[namelen] = '\0';

    if (setenv(name, eq + 1, 1) != 0)
        perror("setenv");

    free(name);
}

int main(int argc, char *argv[]) {
    int opt;
    /* ':' в начале — getopt возвращает ':' при отсутствии аргумента у опции,
       а не '?'. opterr = 0 — сами печатаем сообщения об ошибках. */
    opterr = 0;

    while ((opt = getopt(argc, argv, ":ispuU:cC:dvV:")) != -1) {
        switch (opt) {
            case 'i':
                print_ids();
                break;
            case 's':
                if (setpgid(0, 0) != 0)
                    perror("setpgid");
                else
                    printf("became process group leader, pgid=%d\n", (int)getpgrp());
                break;
            case 'p':
                print_pids();
                break;
            case 'u':
                print_ulimit();
                break;
            case 'U':
                set_ulimit(optarg);
                break;
            case 'c':
                print_core();
                break;
            case 'C':
                set_core(optarg);
                break;
            case 'd':
                print_cwd();
                break;
            case 'v':
                print_env();
                break;
            case 'V':
                set_env(optarg);
                break;
            case ':':   /* отсутствует обязательный аргумент */
                fprintf(stderr, "option -%c requires an argument\n", optopt);
                break;
            case '?':   /* неизвестная опция */
                if (optopt != 0)
                    fprintf(stderr, "invalid option: -%c\n", optopt);
                else
                    fprintf(stderr, "invalid option\n");
                break;
            default:
                break;
        }
    }

    /* Необязательно: показать оставшиеся аргументы (не-опции) */
    if (optind < argc) {
        fprintf(stderr, "non-option arguments:");
        for (int i = optind; i < argc; i++)
            fprintf(stderr, " %s", argv[i]);
        fprintf(stderr, "\n");
    }

    return 0;
}
