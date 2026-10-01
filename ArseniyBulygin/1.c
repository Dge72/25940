#define _GNU_SOURCE
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

#if defined(__sun) || defined(__sunos)
#include <rctl.h>
#include <sys/rctl.h>
#endif

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

#if defined(__sun) || defined(__sunos)
static int get_task_max_processes(unsigned long long *out) {
    rctlblk_t *blk = malloc(rctlblk_size());
    if (!blk) return -1;

    int found = 0;
    if (getrctl("task.max-processes", NULL, blk, RCTL_FIRST) == 0) {
        do {
            if (rctlblk_get_privilege(blk) == RCPRIV_BASIC) {
                *out = (unsigned long long)rctlblk_get_value(blk);
                found = 1;
                break;
            }
        } while (getrctl("task.max-processes", blk, blk, RCTL_NEXT) == 0);
    }
    free(blk);
    return found ? 0 : -1;
}

static int set_task_max_processes(unsigned long long v) {
    rctlblk_t *blk = malloc(rctlblk_size());
    if (!blk) return -1;

    int rc = -1;
    if (getrctl("task.max-processes", NULL, blk, RCTL_FIRST) == 0) {
        do {
            if (rctlblk_get_privilege(blk) == RCPRIV_BASIC) {
                rctlblk_set_value(blk, (rctl_qty_t)v);
                if (setrctl("task.max-processes", blk, RCTL_REPLACE) == 0)
                    rc = 0;
                break;
            }
        } while (getrctl("task.max-processes", blk, blk, RCTL_NEXT) == 0);
    }
    free(blk);
    return rc;
}
#endif

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
#if defined(__sun) || defined(__sunos)
    unsigned long long v;
    if (get_task_max_processes(&v) == 0)
        printf("%llu\n", v);
    else
        printf("unlimited\n");
#else
    struct rlimit rl;
    if (getrlimit(RLIMIT_NPROC, &rl) == 0) {
        print_rlim_value(rl.rlim_cur);
        printf("\n");
    } else {
        perror("getrlimit(RLIMIT_NPROC)");
    }
#endif
}

static void set_rlimit_soft(int resource, const char *val, const char *name) {
    char *end;
    long v;
    struct rlimit rl;

    errno = 0;
    v = strtol(val, &end, 10);
    if (errno != 0 || end == val || *end != '\0' || v < 0) {
        fprintf(stderr, "invalid %s value: '%s'\n", name, val);
        return;
    }

    if (getrlimit(resource, &rl) != 0) {
        perror("getrlimit");
        return;
    }
    rl.rlim_cur = (rlim_t)v;
    if (setrlimit(resource, &rl) != 0)
        perror("setrlimit");
}

void set_ulimit(const char *val) {
#if defined(__sun) || defined(__sunos)
    char *end;
    long v;
    errno = 0;
    v = strtol(val, &end, 10);
    if (errno != 0 || end == val || *end != '\0' || v <= 0) {
        fprintf(stderr, "invalid ulimit value: '%s'\n", val);
        return;
    }
    if (set_task_max_processes((unsigned long long)v) != 0)
        fprintf(stderr, "set_ulimit: cannot set task.max-processes\n");
#else
    set_rlimit_soft(RLIMIT_NPROC, val, "ulimit");
#endif
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
    set_rlimit_soft(RLIMIT_CORE, val, "core size");
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
            case ':':
                fprintf(stderr, "option -%c requires an argument\n", optopt);
                break;
            case '?':
                if (optopt != 0)
                    fprintf(stderr, "invalid option: -%c\n", optopt);
                else
                    fprintf(stderr, "invalid option\n");
                break;
            default:
                break;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "non-option arguments:");
        for (int i = optind; i < argc; i++)
            fprintf(stderr, " %s", argv[i]);
        fprintf(stderr, "\n");
    }

    return 0;
}
