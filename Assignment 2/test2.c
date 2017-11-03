#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int mask = 0777;
    struct stat info;
    lstat(argv[1], &info);
    int perm = info.st_mode & mask;
    printf("%d\n", mask);
    printf("%o\n", info.st_mode);
    printf("%o\n", perm);
}
