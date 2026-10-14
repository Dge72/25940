/* task6.c — таймер 5 секунд только на ПЕРВЫЙ ввод номера */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct { off_t off, len; } lineinfo_t;

static lineinfo_t *table = NULL;
static size_t nlines = 0, capacity = 0;
static volatile sig_atomic_t timed_out = 0;

static void on_alarm(int signo) { (void)signo; timed_out = 1; }

static void table_add(off_t off, off_t len)
{
    if (nlines == capacity) {
        capacity = capacity ? 2 * capacity : 64;
        table = realloc(table, capacity * sizeof *table);
        if (!table) { perror("realloc"); exit(EXIT_FAILURE); }
    }
    table[nlines].off = off;
    table[nlines].len = len;
    nlines++;
}

static void print_line(int fd, size_t num)
{
    lineinfo_t *li = &table[num - 1];
    char *buf;
    off_t got = 0;

    if (lseek(fd, li->off, SEEK_SET) == (off_t)-1) { perror("lseek"); return; }
    buf = malloc((size_t)li->len + 1);
    if (!buf) { perror("malloc"); return; }
    while (got < li->len) {
        ssize_t k = read(fd, buf + got, (size_t)(li->len - got));
        if (k < 0) { if (errno == EINTR) continue; perror("read"); free(buf); return; }
        if (k == 0) break;
        got += k;
    }
    printf("%.*s", (int)got, buf);
    if (got == 0 || buf[got - 1] != '\n') putchar('\n');
    free(buf);
}

static void print_whole_file(int fd)
{
    char buf[4096], last = '\n';
    ssize_t n;

    printf("\nВремя истекло! Печатаю весь файл:\n");
    fflush(stdout);

    off_t sz = lseek(fd, 0L, SEEK_END);
    if (sz > 0 && lseek(fd, -1, SEEK_END) != (off_t)-1)
        (void)read(fd, &last, 1);

    if (lseek(fd, 0L, SEEK_SET) == (off_t)-1) { perror("lseek"); return; }
    while ((n = read(fd, buf, sizeof buf)) > 0)
        if (write(STDOUT_FILENO, buf, (size_t)n) == -1) break;
    if (last != '\n') putchar('\n');
}

int main(int argc, char *argv[])
{
    int fd, saved_errno;
    char buf[4096], inbuf[64];
    struct sigaction sa;
    int first = 1;                       

    if (argc != 2) { fprintf(stderr, "Использование: %s файл\n", argv[0]); return 1; }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) { perror("open"); return 1; }

    /* таблица */
    {
        off_t line_start = 0;
        for (;;) {
            ssize_t n = read(fd, buf, sizeof buf);
            if (n == -1) { perror("read"); return 1; }
            if (n == 0) break;
            off_t cur = lseek(fd, 0L, SEEK_CUR);
            off_t base = cur - n;
            for (ssize_t i = 0; i < n; i++)
                if (buf[i] == '\n') {
                    off_t next = base + i + 1;
                    table_add(line_start, next - line_start);
                    line_start = next;
                }
        }
        off_t eof = lseek(fd, 0L, SEEK_CUR);
        if (eof > line_start) table_add(line_start, eof - line_start);
    }

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) == -1) { perror("sigaction"); return 1; }

    for (;;) {
        long num;
        char *end;
        ssize_t total = 0;
        int got_eof = 0, hit_timeout = 0;

        printf("Номер строки%s (0 — выход): ",
            first ? " (5 секунд на ввод)" : "");
        fflush(stdout);

        if (first) { timed_out = 0; alarm(5); }   /* таймер только на первый ввод */

        for (;;) {
            ssize_t r = read(STDIN_FILENO, inbuf + total, 1);
            saved_errno = errno;
            if (r == -1) {
                if (saved_errno == EINTR && timed_out) { hit_timeout = 1; break; }
                if (saved_errno == EINTR) continue;
                perror("read");
                alarm(0);
                close(fd);
                free(table);
                return 1;
            }
            if (r == 0) { got_eof = 1; break; }
            if (inbuf[total] == '\n') { total++; break; }
            total++;
            if (total >= (ssize_t)sizeof inbuf - 1) break;
        }

        if (first) { alarm(0); first = 0; }       

        if (hit_timeout) { print_whole_file(fd); break; }
        if (got_eof && total == 0) { putchar('\n'); break; }

        inbuf[total] = '\0';
        num = strtol(inbuf, &end, 10);
        if (end == inbuf) { printf("Некорректный ввод.\n"); continue; }
        if (num == 0) break;
        if (num < 1 || (size_t)num > nlines) {
            printf("В файле всего %zu строк(и).\n", nlines);
            continue;
        }
        print_line(fd, (size_t)num);
    }

    close(fd);
    free(table);
    return 0;
}