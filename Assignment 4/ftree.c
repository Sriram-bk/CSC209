#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include "ftree.h"


/*
    Extracts and returns the file name from path.
*/
char *get_name(const char *path) {
    const char slash = '/';
    char *name = malloc(MAXPATH * sizeof(char));
    memset(name, '\0', MAXPATH);
    if(strrchr(path, slash) == NULL) {
        strncpy(name, path, MAXPATH);
    }
    else {
        strncpy(name, strrchr(path, slash)+1, MAXPATH);
    }
    return name;
}

/*
    Concatenates a '/' and part2 to part1 to create a file path.
    Returns the newly created string.
*/
char *get_path(const char *part1, const char *part2, int len) {

    char *path = malloc(len * sizeof(char));
    int part1_len = strlen(part1);
    int part2_len = strlen(part2);
    memset(path, '\0', len);
    strncpy(path, part1, part1_len);
    strncat(path, "/", 1);
    strncat(path, part2, part2_len);

    return path;
}
