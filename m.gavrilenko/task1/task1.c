#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>      
#include <sys/resource.h>
#include <sys/rctl.h>      
#include <rctl.h>                            

extern char **environ;

static const char *progname = "task1";

static const char *lwps_candidates[] = {
    "task.max-lwps",
    "task.max-processes",
    "project.max-lwps",
    "project.max-processes",
    "zone.max-lwps",
    "zone.max-processes",
    NULL
};

static const char *lwps_name_cache = NULL;
static int         lwps_name_tried = 0;

static const char *
lwps_rctl_name(void)
{
    rctlblk_t *blk;
    int        i;

    if (lwps_name_tried)
        return lwps_name_cache;
    lwps_name_tried = 1;

    blk = (rctlblk_t *)malloc(rctlblk_size());
    if (blk == NULL)
        return NULL;

    for (i = 0; lwps_candidates[i] != NULL; i++) {
        errno = 0;
        if (getrctl(lwps_candidates[i], NULL, blk, RCTL_FIRST) == 0) {
            lwps_name_cache = lwps_candidates[i];
            break;
        }
    }
    free(blk);
    return lwps_name_cache;
}

typedef struct {
    int   opt;
    char *arg;
} action_t;

static void usage(void)
{
    fprintf(stderr,
        "Использование: %s [-i] [-s] [-p] [-u] [-U n] [-c] [-C n] "
        "[-d] [-v] [-V name=value]\n", progname);
}

static void print_ids(void)
{
    printf("реальный    uid = %ld\n", (long)getuid());
    printf("эффективный uid = %ld\n", (long)geteuid());
    printf("реальный    gid = %ld\n", (long)getgid());
    printf("эффективный gid = %ld\n", (long)getegid());
}

static void print_pids(void)
{
    printf("pid  = %ld\n", (long)getpid());
    printf("ppid = %ld\n", (long)getppid());
    printf("pgid = %ld\n", (long)getpgrp());
}

/* Печатает все локальные значения одного rctl — для справки. */
static void print_rctl_block(const char *name)
{
    rctlblk_t *blk;
    int        first = 1;

    blk = (rctlblk_t *)malloc(rctlblk_size());
    if (blk == NULL) { perror("malloc"); return; }

    if (getrctl(name, NULL, blk, RCTL_FIRST) == -1) {
        free(blk);
        return;
    }
    do {
        rctl_priv_t        priv   = rctlblk_get_privilege(blk);
        unsigned long long val    = (unsigned long long)rctlblk_get_value(blk);
        uint_t             lflags = rctlblk_get_local_flags(blk);
        int                sig;
        int                act    = rctlblk_get_local_action(blk, &sig);
        const char        *pstr   = priv == RCPRIV_BASIC      ? "basic" :
                                    priv == RCPRIV_PRIVILEGED ? "privileged" :
                                                                "system";

        if (first) { printf("rctl %s:\n", name); first = 0; }
        if (lflags & RCTL_LOCAL_MAXIMAL) {
            printf("  %-11s %12s   (максимально возможное)\n", pstr, "-");
        } else {
            const char *actstr =
                (act & RCTL_LOCAL_DENY)   ? "deny"   :
                (act & RCTL_LOCAL_SIGNAL) ? "signal" : "noaction";
            printf("  %-11s %12llu   %s\n", pstr, val, actstr);
        }
    } while (getrctl(name, NULL, blk, RCTL_NEXT) == 0);

    free(blk);
}


static void print_max_procs(void)
{
#ifdef RLIMIT_NPROC
        {
        struct rlimit rl;
        if (getrlimit(RLIMIT_NPROC, &rl) == -1) {
            perror("getrlimit(RLIMIT_NPROC)");
        } else if (rl.rlim_cur == RLIM_INFINITY) {
            printf("RLIMIT_NPROC (ulimit -u, soft): без ограничений\n");
        } else {
            printf("RLIMIT_NPROC (ulimit -u, soft) = %llu\n",
                (unsigned long long)rl.rlim_cur);
        }
    }
#endif

    
        long n = sysconf(_SC_CHILD_MAX);
        if (n > 0)
            printf("sysconf(_SC_CHILD_MAX) (ulimit -u) = %ld\n", n);
        else
            printf("sysconf(_SC_CHILD_MAX): недоступно\n");
    

    /* Справочно: rctl task.max-lwps (per-task) либо первый доступный. */
    {
        const char *name = lwps_rctl_name();
        if (name != NULL)
            print_rctl_block(name);
        else
            fprintf(stderr,
                "%s: ни один из rctl (task/project/zone.max-lwps) "
                "не зарегистрирован в ядре\n", progname);
    }
}


static void set_max_procs(const char *arg)
{
    char              *end;
    long               v;
    const char        *name;
    rctlblk_t         *newblk = NULL, *cur = NULL, *basic_blk = NULL;
    size_t             sz;
    unsigned long long usage = 0;

    errno = 0;
    v = strtol(arg, &end, 10);
    if (end == arg || *end != '\0' || v < 0 ||
        (unsigned long long)v > 0xFFFFFFFFULL) {
        fprintf(stderr, "%s: -U: \"%s\" — некорректное значение\n",
                progname, arg);
        return;
    }

#ifdef RLIMIT_NPROC
    {
        struct rlimit rl;
        if (getrlimit(RLIMIT_NPROC, &rl) == -1) {
            perror("getrlimit(RLIMIT_NPROC)");
        } else if (rl.rlim_max != RLIM_INFINITY &&
                (unsigned long long)v > (unsigned long long)rl.rlim_max) {
            fprintf(stderr,
                "%s: -U %ld: превышает hard-лимит RLIMIT_NPROC = %llu\n",
                progname, v, (unsigned long long)rl.rlim_max);
            return;
        } else {
            rl.rlim_cur = (rlim_t)v;
            if (setrlimit(RLIMIT_NPROC, &rl) == 0) {
                printf("RLIMIT_NPROC (ulimit -u, soft) изменён на %ld\n", v);
                return;
            }
            perror("setrlimit(RLIMIT_NPROC)");
        }
    }
#endif

    name = lwps_rctl_name();
    if (name == NULL) {
        fprintf(stderr,
            "%s: -U: ни task.max-lwps, ни project.max-lwps, ни "
            "zone.max-lwps не зарегистрированы;\n"
            "        изменить ulimit -u из программы в этой системе нельзя.\n",
            progname);
        return;
    }

    sz     = rctlblk_size();
    newblk = (rctlblk_t *)malloc(sz);
    cur    = (rctlblk_t *)malloc(sz);
    if (newblk == NULL || cur == NULL) { perror("malloc"); goto out; }

    if (getrctl(name, NULL, cur, RCTL_USAGE) == 0) {
        usage = (unsigned long long)rctlblk_get_value(cur);
        if ((unsigned long long)v < usage) {
            fprintf(stderr,
                "%s: -U %ld: сейчас уже используется %llu; значение "
                "должно быть не меньше\n", progname, v, usage);
            goto out;
        }
    }

    if (getrctl(name, NULL, cur, RCTL_FIRST) == 0) {
        do {
            if (rctlblk_get_privilege(cur) == RCPRIV_BASIC) {
                basic_blk = (rctlblk_t *)malloc(sz);
                if (basic_blk != NULL) memcpy(basic_blk, cur, sz);
                break;
            }
        } while (getrctl(name, NULL, cur, RCTL_NEXT) == 0);
    }

    memset(newblk, 0, sz);
    rctlblk_set_privilege(newblk, RCPRIV_BASIC);
    rctlblk_set_value(newblk, (rctl_qty_t)v);
    rctlblk_set_local_flags(newblk, 0);
    rctlblk_set_local_action(newblk, RCTL_LOCAL_DENY, 0);

    if (basic_blk != NULL ? setrctl(name, basic_blk, newblk, RCTL_REPLACE)
                        : setrctl(name, NULL,     newblk, RCTL_INSERT)) {
        perror("setrctl basic");
        fprintf(stderr,
            "  подсказка: basic-запись на %s может быть запрещена (флаг "
            "no-basic в `rctladm -l`); в этом случае её может поставить "
            "только project/zone admin\n", name);
    } else {
        printf("rctl %s (basic) изменён на %ld\n", name, v);
    }

out:
    free(newblk);
    free(cur);
    free(basic_blk);
}

static void do_action(const action_t *a)
{
    struct rlimit rl;
    char          cwd[4096];

    switch (a->opt) {
    case 'i': print_ids(); break;
    case 's':
        if (setpgid(0, 0) == -1) perror("setpgid");
        break;
    case 'p': print_pids(); break;
    case 'u': print_max_procs(); break;

    case 'U':
        if (a->arg == NULL) usage();
        else                set_max_procs(a->arg);
        break;

    case 'c':
        if (getrlimit(RLIMIT_CORE, &rl) == -1)
            perror("getrlimit");
        else if (rl.rlim_cur == RLIM_INFINITY)
            printf("размер core-файла: без ограничений\n");
        else
            printf("размер core-файла = %llu байт\n",
                (unsigned long long)rl.rlim_cur);
        break;

    case 'C':
        if (a->arg == NULL) { usage(); break; }
        if (getrlimit(RLIMIT_CORE, &rl) == -1) { perror("getrlimit"); break; }
        rl.rlim_cur = (rlim_t)atol(a->arg);
        if (setrlimit(RLIMIT_CORE, &rl) == -1) perror("setrlimit");
        break;

    case 'd':
        if (getcwd(cwd, sizeof cwd) == NULL) perror("getcwd");
        else                                 printf("текущий каталог: %s\n", cwd);
        break;

    case 'v': {
        char **e;
        for (e = environ; *e != NULL; e++) printf("%s\n", *e);
        break;
    }

    case 'V': {
        char *s;
        if (a->arg == NULL || strchr(a->arg, '=') == NULL) {
            fprintf(stderr, "%s: -V: ожидается name=value, получено \"%s\"\n",
                    progname, a->arg ? a->arg : "");
            break;
        }
        s = (char *)malloc(strlen(a->arg) + 1);
        if (s == NULL) { perror("malloc"); break; }
        strcpy(s, a->arg);
        if (putenv(s) != 0) perror("putenv");
        break;
    }

    default:
        usage();
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    action_t *acts = NULL;
    size_t    n = 0, cap = 0;
    int       c;

    if (argc > 0) progname = argv[0];

    while ((c = getopt(argc, argv, "ispuU:cC:dvV:")) != -1) {
        if (n == cap) {
            action_t *tmp;
            cap = cap ? 2 * cap : 16;
            tmp = (action_t *)realloc(acts, cap * sizeof *tmp);
            if (tmp == NULL) { perror("realloc"); free(acts); return EXIT_FAILURE; }
            acts = tmp;
        }
        acts[n].opt = c;
        acts[n].arg = (c == 'U' || c == 'C' || c == 'V') ? optarg : NULL;
        n++;
    }

    while (n > 0) do_action(&acts[--n]);

    free(acts);
    return 0;
}
