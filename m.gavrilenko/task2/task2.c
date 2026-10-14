/* task2.c — выводит UTC, PST (UTC-8) и PDT (UTC-7) */
#include <stdio.h>
#include <time.h>

int main(void)
{
    time_t now = time(NULL);
    struct tm tm_utc, tm_pst, tm_pdt;
    char s_utc[64], s_pst[64], s_pdt[64];

    time_t t_pst = now - 8L * 3600L;   /* PST = UTC - 8 */
    time_t t_pdt = now - 7L * 3600L;   /* PDT = UTC - 7 */

    if (gmtime_r(&now,   &tm_utc) == NULL ||
        gmtime_r(&t_pst, &tm_pst) == NULL ||
        gmtime_r(&t_pdt, &tm_pdt) == NULL) {
        fprintf(stderr, "ошибка gmtime_r\n");
        return 1;
    }

    if (strftime(s_utc, sizeof s_utc, "%d.%m.%Y %H:%M:%S UTC", &tm_utc) == 0 ||
        strftime(s_pst, sizeof s_pst, "%d.%m.%Y %H:%M:%S PST", &tm_pst) == 0 ||
        strftime(s_pdt, sizeof s_pdt, "%d.%m.%Y %H:%M:%S PDT", &tm_pdt) == 0) {
        fprintf(stderr, "ошибка strftime\n");
        return 1;
    }

    printf("UTC : %s\n", s_utc);
    printf("PST : %s\n", s_pst);
    printf("PDT : %s\n", s_pdt);
    return 0;
}