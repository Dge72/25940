// task7.c — mmap 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

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

int main(int argc, char *argv[])
{
    int fd, saved_errno;
    struct stat st;
    char *map;
    struct sigaction sa;
    int first = 1;                       

    if (argc != 2) { fprintf(stderr, "Использование: %s файл\n", argv[0]); return 1; }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) { perror("open"); return 1; }
    if (fstat(fd, &st) == -1) { perror("fstat"); return 1; }
    if (st.st_size == 0) { fprintf(stderr, "Файл пуст.\n"); return 1; }

    map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); return 1; }
    close(fd);

    
    {
        off_t line_start = 0;
        for (off_t i = 0; i < st.st_size; i++)
            if (map[i] == '\n') {
                table_add(line_start, i + 1 - line_start);
                line_start = i + 1;
            }
        if (line_start < st.st_size)
            table_add(line_start, st.st_size - line_start);
    }

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    for (;;) {
        char inbuf[64], *end;
        long num;
        ssize_t total = 0;
        int got_eof = 0, hit_timeout = 0;

        printf("Номер строки%s (0 — выход): ",
            first ? " (5 секунд на ввод)" : "");
        fflush(stdout);

        if (first) { timed_out = 0; alarm(5); }   

        for (;;) {
            ssize_t r = read(STDIN_FILENO, inbuf + total, 1);
            saved_errno = errno;
            if (r == -1) {
                if (saved_errno == EINTR && timed_out) { hit_timeout = 1; break; }
                if (saved_errno == EINTR) continue;
                perror("read");
                alarm(0);
                munmap(map, (size_t)st.st_size);
                free(table);
                return 1;
            }
            if (r == 0) { got_eof = 1; break; }
            if (inbuf[total] == '\n') { total++; break; }
            total++;
            if (total >= (ssize_t)sizeof inbuf - 1) break;
        }

        if (first) { alarm(0); first = 0; }       

        if (hit_timeout) {
            printf("\nВремя истекло! Печатаю весь файл:\n");
            fwrite(map, 1, (size_t)st.st_size, stdout);
            if (st.st_size > 0 && map[st.st_size - 1] != '\n')
                putchar('\n');
            break;
        }
        if (got_eof && total == 0) { putchar('\n'); break; }

        inbuf[total] = '\0';
        num = strtol(inbuf, &end, 10);
        if (end == inbuf) { printf("Некорректный ввод.\n"); continue; }
        if (num == 0) break;
        if (num < 1 || (size_t)num > nlines) {
            printf("В файле всего %zu строк(и).\n", nlines);
            continue;
        }

        lineinfo_t *li = &table[num - 1];
        printf("%.*s", (int)li->len, map + li->off);
        if (li->len == 0 || map[li->off + li->len - 1] != '\n')
            putchar('\n');
    }

    munmap(map, (size_t)st.st_size);
    free(table);
    return 0;
}