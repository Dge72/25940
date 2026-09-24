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

#define MAX_ACTIONS 256

struct action {
    int opt;
    char *arg;
};

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



void do_i(void) {
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

void do_s(void) {
    if (setpgid(0, 0) != 0)
        perror("setpgid");
    else
        printf("became process group leader, pgid=%d\n", (int)getpgrp());
}

void do_p(void) {
    printf("pid=%d\n",  (int)getpid());
    printf("ppid=%d\n", (int)getppid());
    printf("pgid=%d\n", (int)getpgrp());
}


void do_u(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_FSIZE, &rl) == 0) {
        print_rlim_value(rl.rlim_cur);
        printf("\n");
    } else {
        perror("getrlimit(RLIMIT_FSIZE)");
    }
}


void do_U(const char *val) {
    char *end;
    long v;
    struct rlimit rl;

    if (val == NULL) {
        fprintf(stderr, "-U requires a value\n");
        return;
    }

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

/* -c: размер core-файла в байтах */
void do_c(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == 0) {
        print_rlim_value(rl.rlim_cur);
        printf("\n");
    } else {
        perror("getrlimit(RLIMIT_CORE)");
    }
}


void do_C(const char *val) {
    char *end;
    long v;
    struct rlimit rl;

    if (val == NULL) {
        fprintf(stderr, "-C requires a value\n");
        return;
    }

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

void do_d(void) {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) != NULL)
        printf("cwd=%s\n", buf);
    else
        perror("getcwd");
}

void do_v(void) {
    for (char **e = environ; *e != NULL; e++)
        printf("%s\n", *e);
}

void do_V(const char *arg) {
    char *eq;
    size_t namelen;
    char *name;

    if (arg == NULL) {
        fprintf(stderr, "-V requires a value\n");
        return;
    }

    eq = strchr(arg, '=');
    if (eq == NULL || eq == arg) {
        fprintf(stderr, "invalid -V format (expected name=value): '%s'\n", arg);
        return;
    }

    namelen = (size_t)(eq - arg);
    name = malloc(namelen + 1);
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
    struct action actions[MAX_ACTIONS];
    int n = 0;

    opterr = 0;

    while ((opt = getopt(argc, argv, ":ispuU:cC:dvV:")) != -1) {
        if (n >= MAX_ACTIONS) {
            fprintf(stderr, "too many options\n");
            return 1;
        }
        if (opt == ':' || opt == '?') {

            if (opt == ':')
                fprintf(stderr, "option -%c requires an argument\n", optopt);
            else if (optopt != 0)
                fprintf(stderr, "invalid option: -%c\n", optopt);
            else
                fprintf(stderr, "invalid option\n");
            continue;
        }
        actions[n].opt = opt;
        actions[n].arg = optarg;
        n++;
    }

    for (int k = n - 1; k >= 0; k--) {
        switch (actions[k].opt) {
            case 'i': do_i();            break;
            case 's': do_s();            break;
            case 'p': do_p();            break;
            case 'u': do_u();            break;
            case 'U': do_U(actions[k].arg); break;
            case 'c': do_c();            break;
            case 'C': do_C(actions[k].arg); break;
            case 'd': do_d();            break;
            case 'v': do_v();            break;
            case 'V': do_V(actions[k].arg); break;
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
