#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
    // Your implementation here.
    struct stat info;

    if (lstat(argv[1], &info) == 0) {
        if(S_ISDIR(info.st_mode)){
            printf("It's a directory\n");
        }
        else if(S_ISREG(info.st_mode)){
            printf("Its a File\n");
        }
    }


    return 0;
}
