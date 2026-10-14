#include <stdio.h>
#include <unistd.h>
#include <errno.h>
int main(void)
{
    FILE *fp;
    
    printf("=== Первый запуск ===\n");
    printf("Реальный UID    (RUID) = %d\n", getuid());
    printf("Эффективный UID (EUID) = %d\n", geteuid());
    
    fp = fopen("datafile", "r");
    if (fp == NULL) {
        perror("fopen(datafile)");
    } else {
        printf("fopen(datafile): успешно открыт\n");
        fclose(fp);
    }
    
    if (setuid(getuid()) == -1) {
        perror("setuid");
    } else {
        printf("setuid выполнен: RUID теперь равен EUID\n");
    }
    
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