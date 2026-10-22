#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

static int fd_global = -1;

static void on_alarm(int sig)
{
    (void)sig;

    if (fd_global < 0)
        _exit(1);

    if (lseek(fd_global, 0L, SEEK_SET) == (off_t)-1)
        _exit(1);

    char buf[4096];
    ssize_t n;
    while ((n = read(fd_global, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, (size_t)n);

    _exit(0);
}

static void sanitize(char *s)
{
    char *in = s;
    char *out = s;

    while (*in) {
        unsigned char c = (unsigned char)*in;

        if (c == 0x1B) {
            in++;
            if (*in == '[') {
                in++;
                while (*in && !((unsigned char)*in >= '@'
                                && (unsigned char)*in <= '~'))
                    in++;
                if (*in) in++;
            } else if (*in == 'O') {
                in++;
                if (*in) in++;
            } else {
                if (*in) in++;
            }
            continue;
        }

        if (c < 0x20 && c != '\n' && c != '\t') {
            in++;
            continue;
        }

        if (c == 0x7F) {
            in++;
            continue;
        }

        *out++ = *in++;
    }
    *out = '\0';
}

struct line_entry {
    off_t offset;
    size_t length;
};

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    fd_global = fd;

    size_t cap = 16;
    size_t count = 0;
    struct line_entry *table = malloc(cap * sizeof(*table));
    if (table == NULL) {
        perror("malloc");
        close(fd);
        return 1;
    }

    off_t pos = 0;
    off_t line_start = 0;
    char c;
    ssize_t n;

    while ((n = read(fd, &c, 1)) == 1) {
        pos++;
        if (c == '\n') {
            if (count == cap) {
                cap *= 2;
                struct line_entry *tmp = realloc(table, cap * sizeof(*table));
                if (tmp == NULL) {
                    perror("realloc");
                    free(table);
                    close(fd);
                    return 1;
                }
                table = tmp;
            }
            table[count].offset = line_start;
            table[count].length = (size_t)(pos - line_start);
            count++;
            line_start = pos;
        }
    }
    if (n < 0) {
        perror("read");
        free(table);
        close(fd);
        return 1;
    }

    if (pos > line_start) {
        if (count == cap) {
            cap += 1;
            struct line_entry *tmp = realloc(table, cap * sizeof(*table));
            if (tmp == NULL) {
                perror("realloc");
                free(table);
                close(fd);
                return 1;
            }
            table = tmp;
        }
        table[count].offset = line_start;
        table[count].length = (size_t)(pos - line_start);
        count++;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) != 0) {
        perror("sigaction");
        free(table);
        close(fd);
        return 1;
    }

    int first_prompt = 1;

    for (;;) {
        printf("Enter line number (0 to quit): ");
        fflush(stdout);

        if (first_prompt)
            alarm(5);

        char buf[64];
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            alarm(0);
            first_prompt = 0;
            clearerr(stdin);
            printf("\n");
            continue;
        }

        alarm(0);
        first_prompt = 0;

        sanitize(buf);

        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
            len--;
        }

        if (len == 0) {
            printf("No such line\n");
            continue;
        }

        char *end;
        errno = 0;
        long num = strtol(buf, &end, 10);

        if (errno != 0 || end == buf || *end != '\0') {
            printf("No such line\n");
            continue;
        }

        if (num == 0)
            break;

        if (num < 0 || (size_t)num > count) {
            printf("No such line\n");
            continue;
        }

        size_t idx = (size_t)num - 1;

        if (lseek(fd, table[idx].offset, SEEK_SET) == (off_t)-1) {
            perror("lseek");
            continue;
        }

        char *line = malloc(table[idx].length + 1);
        if (line == NULL) {
            perror("malloc");
            continue;
        }

        ssize_t got = read(fd, line, table[idx].length);
        if (got < 0) {
            perror("read");
            free(line);
            continue;
        }
        line[got] = '\0';

        printf("%s", line);
        if (got == 0 || line[got - 1] != '\n')
            printf("\n");

        free(line);
    }

    free(table);
    close(fd);
    return 0;
}