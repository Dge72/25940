#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
    off_t off;   
    off_t len;   
} lineinfo_t;

static lineinfo_t *table = NULL;
static size_t nlines = 0, capacity = 0;

static void table_add(off_t off, off_t len)
{
    if (nlines == capacity) {
        capacity = capacity ? 2 * capacity : 64;
        table = realloc(table, capacity * sizeof *table);
        if (table == NULL) { perror("realloc"); exit(EXIT_FAILURE); }
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

    if (lseek(fd, li->off, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        return;
    }
    buf = malloc((size_t)li->len + 1);
    if (buf == NULL) { perror("malloc"); return; }

    while (got < li->len) {
        ssize_t k = read(fd, buf + got, (size_t)(li->len - got));
        if (k < 0) {
            if (errno == EINTR) continue;
            perror("read");
            free(buf);
            return;
        }
        if (k == 0) break;
        got += k;
    }
    printf("%.*s", (int)got, buf);
    if (got == 0 || buf[got - 1] != '\n')
        putchar('\n');
    free(buf);
}

int main(int argc, char *argv[])
{
    int  fd, show_table = 0;
    char buf[4096];

    if (argc < 2) {
        fprintf(stderr, "Использование: %s файл [-t]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 3 && strcmp(argv[2], "-t") == 0)
        show_table = 1;

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) { perror("open"); return EXIT_FAILURE; }

    
    {
        off_t line_start = 0;

        for (;;) {
            ssize_t n = read(fd, buf, sizeof buf);
            if (n == -1) { perror("read"); return EXIT_FAILURE; }
            if (n == 0) break;

            off_t cur  = lseek(fd, 0L, SEEK_CUR);
            off_t base = cur - n;

            for (ssize_t i = 0; i < n; i++)
                if (buf[i] == '\n') {
                    off_t next = base + i + 1;
                    table_add(line_start, next - line_start);
                    line_start = next;
                }
        }
        off_t eof = lseek(fd, 0L, SEEK_CUR);
        if (eof > line_start)
            table_add(line_start, eof - line_start);
    }

    if (show_table) {
        printf("--- таблица: %zu строк ---\n", nlines);
        for (size_t i = 0; i < nlines; i++)
            printf("строка %3zu: смещение = %5ld, длина = %4ld\n",
                i + 1, (long)table[i].off, (long)table[i].len);
    }

    for (;;) {
        long num;
        printf("Введите номер строки (0 — выход): ");
        fflush(stdout);

        if (scanf("%ld", &num) != 1) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;
            if (ch == EOF) break;
            printf("Некорректный ввод.\n");
            continue;
        }
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