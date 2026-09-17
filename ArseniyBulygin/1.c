#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>

extern char **environ;

void print_ids(void) {
    struct passwd *pw;
    struct group *gr;
    uid_t ruid = getuid(), euid = geteuid();
    gid_t rgid = getgid(), egid = getegid();

    printf("real uid=%d, effective uid=%d\n", ruid, euid);
    printf("real gid=%d, effective gid=%d\n", rgid, egid);

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
    printf("pid=%d\n", getpid());
    printf("ppid=%d\n", getppid());
    printf("pgid=%d\n", getpgrp());
}

void print_ulimit(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_FSIZE, &rl) == 0)
        printf("ulimit (RLIMIT_FSIZE): soft=%ld, hard=%ld\n",
               (long)rl.rlim_cur, (long)rl.rlim_max);
}

void set_ulimit(const char *val) {
    struct rlimit rl;
    long v = atol(val);
    if (v < 0) { fprintf(stderr, "invalid ulimit value\n"); return; }
    if (getrlimit(RLIMIT_FSIZE, &rl) == 0) {
        rl.rlim_cur = v;
        if (setrlimit(RLIMIT_FSIZE, &rl) != 0)
            perror("setrlimit");
    }
}

void print_core(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == 0)
        printf("core size: soft=%ld, hard=%ld\n",
               (long)rl.rlim_cur, (long)rl.rlim_max);
}

void set_core(const char *val) {
    struct rlimit rl;
    long v = atol(val);
    if (v < 0) { fprintf(stderr, "invalid core size\n"); return; }
    if (getrlimit(RLIMIT_CORE, &rl) == 0) {
        rl.rlim_cur = v;
        if (setrlimit(RLIMIT_CORE, &rl) != 0)
            perror("setrlimit");
    }
}

void print_cwd(void) {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)))
        printf("cwd=%s\n", buf);
}

void print_env(void) {
    for (char **e = environ; *e; e++)
        printf("%s\n", *e);
}

void set_env(const char *arg) {
    char *eq = strchr(arg, '=');
    if (!eq) { fprintf(stderr, "invalid -V format\n"); return; }
    if (setenv(arg, eq + 1, 1) != 0)
        perror("setenv");
}

int main(int argc, char *argv[]) {
    int opt;
    opterr = 0;

    while ((opt = getopt(argc, argv, "ispuU:cC:dD:vV:")) != -1) {
        switch (opt) {
            case 'i': print_ids(); break;
            case 's':
                if (setpgid(0, 0) != 0) perror("setpgid");
                else printf("became process group leader, pgid=%d\n", getpgrp());
                break;
            case 'p': print_pids(); break;
            case 'u': print_ulimit(); break;
            case 'U': set_ulimit(optarg); break;
            case 'c': print_core(); break;
            case 'C': set_core(optarg); break;
            case 'd': print_cwd(); break;
            case 'v': print_env(); break;
            case 'V': set_env(optarg); break;
            case '?':
                fprintf(stderr, "invalid option: -%c\n", optopt);
                break;
        }
    }
    return 0;
}