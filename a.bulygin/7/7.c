#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>

static char *map_base = NULL;
static size_t map_size = 0;

static void on_alarm(int sig)
{
    (void)sig;

    if (map_base == NULL)
        _exit(1);

    size_t off = 0;
    while (off < map_size) {
        size_t chunk = map_size - off;
        if (chunk > 4096) chunk = 4096;
        ssize_t w = write(STDOUT_FILENO, map_base + off, chunk);
        if (w <= 0)
            break;
        off += (size_t)w;
    }

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

    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("fstat");
        close(fd);
        return 1;
    }

    if (st.st_size == 0) {
        close(fd);
        printf("File is empty\n");
        return 0;
    }

    map_size = (size_t)st.st_size;

    map_base = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_base == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    close(fd);

    size_t cap = 16;
    size_t count = 0;
    struct line_entry *table = malloc(cap * sizeof(*table));
    if (table == NULL) {
        perror("malloc");
        munmap(map_base, map_size);
        return 1;
    }

    size_t line_start = 0;
    for (size_t i = 0; i < map_size; i++) {
        if (map_base[i] == '\n') {
            if (count == cap) {
                cap *= 2;
                struct line_entry *tmp = realloc(table, cap * sizeof(*table));
                if (tmp == NULL) {
                    perror("realloc");
                    free(table);
                    munmap(map_base, map_size);
                    return 1;
                }
                table = tmp;
            }
            table[count].offset = (off_t)line_start;
            table[count].length = i - line_start + 1;
            count++;
            line_start = i + 1;
        }
    }

    if (line_start < map_size) {
        if (count == cap) {
            cap += 1;
            struct line_entry *tmp = realloc(table, cap * sizeof(*table));
            if (tmp == NULL) {
                perror("realloc");
                free(table);
                munmap(map_base, map_size);
                return 1;
            }
            table = tmp;
        }
        table[count].offset = (off_t)line_start;
        table[count].length = map_size - line_start;
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
        munmap(map_base, map_size);
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

        fwrite(map_base + table[idx].offset, 1, table[idx].length, stdout);
        if (table[idx].length == 0 ||
            map_base[table[idx].offset + table[idx].length - 1] != '\n')
            printf("\n");
    }

    free(table);
    munmap(map_base, map_size);
    return 0;
}