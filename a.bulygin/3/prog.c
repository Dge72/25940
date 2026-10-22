#include <stdio.h>
#include <unistd.h>
#include <errno.h>
int main(void)
{
    FILE *fp;
    /* --- Шаг 1: печатаем реальный и эффективный UID --- */
    printf("=== Первый запуск ===\n");
    printf("Реальный UID    (RUID) = %d\n", getuid());
    printf("Эффективный UID (EUID) = %d\n", geteuid());
    /* --- Шаг 2: пробуем открыть файл --- */
    fp = fopen("datafile", "r");
    if (fp == NULL) {
        perror("fopen(datafile)");
    } else {
        printf("fopen(datafile): успешно открыт\n");
        fclose(fp);
    }
    /* --- Шаг 3: делаем реальный UID = эффективному --- */
    if (setuid(geteuid()) == -1) {
        perror("setuid");
    } else {
        printf("setuid выполнен: RUID теперь равен EUID\n");
    }
    /* --- Шаг 4: повторяем шаги 1 и 2 --- */
    printf("\n=== Второй запуск (после setuid) ===\n");
    printf("Реальный UID    (RUID) = %d\n", getuid());
    printf("Эффективный UID (EUID) = %d\n", geteuid());
    fp = fopen("datafile", "r");
    if (fp == NULL) {
        perror("fopen(datafile)");
    } else {
        printf("fopen(datafile): успешно открыт\n");
        fclose(fp);
    }
    return 0;
}