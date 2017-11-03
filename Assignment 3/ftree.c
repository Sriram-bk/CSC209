#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include "hash.h"

int copy_file(const char *src, const char *dest, mode_t perm, off_t size);
char *get_path(const char *part1, const char *part2, int len);
char *get_name(const char* path);

/*
    Copies over the file tree rooted at src into the directory 'dest'.

    Does not copy over regular files in the file tree rooted at 'src' that
    don't have valid permissions.
*/
int copy_ftree(const char *src, const char *dest) {
    struct stat src_info, dest_info, src_item_info;
    struct dirent *dp;
    char processes = 1;
    int num_children = 0;
    int flag = 0;

    if ((lstat(src, &src_info) != 0) || (lstat(dest, &dest_info) != 0)) {
        perror("lstat");
        return -1;
    }

    if (S_ISREG(dest_info.st_mode)) {
        printf("Can't copy into a file\n");
        return -1;
    }

    DIR *destp = opendir(dest);
    if(destp == NULL) {
        perror("dest");
        return -1;
    }

    // Copy the file 'src' into dest.
    if(S_ISREG(src_info.st_mode)) {
         flag += copy_file(src, dest, src_info.st_mode, src_info.st_size);
         if(flag == -1 && processes > 0) {
             processes = -processes;
         }
    }

    // Copy the directory 'src' into dest.
    else if (S_ISDIR(src_info.st_mode)) {

        char *src_name = get_name(src); // Get the name of the directory.
        int dir_path_len = strlen(dest) + strlen(src_name) + 2;
        char *dir_path = get_path(dest, src_name, dir_path_len);
        free(src_name);

        DIR *dest_dir = opendir(dir_path);
        if(dest_dir == NULL) {
            if(ENOENT == errno || ENOTDIR == errno || EACCES == errno) {
                if(ENOTDIR == errno) { // return error if the types don't match.
                    printf("Type mismatch.\n");
                    free(dir_path);
                    return -1;
                }
                // Create the directory if it doesn't exist.
                else if (ENOENT == errno) {
                    if(mkdir(dir_path, 00777) != 0) {
                        perror("Failed to create directory");
                        return -1;
                    }
                }
                // Modify permissions if dir doesn't have valid permissions.
                else {
                    chmod(dir_path, 00777);
                }
            }
            else { // return error if opendir failed for any other reason.
                perror("opendir");
                return -1;
            }
        }
        else {
            chmod(dir_path, 00777);
            closedir(dest_dir);
        }

        DIR *src_dirp = opendir(src);
    	if(src_dirp == NULL) {
    		perror("source dir");
            if(chmod(dir_path, src_info.st_mode) != 0){
                perror("Directory permissions couldn't be set");
            }
    		return -1;
    	}

        errno = 0;
        dp = readdir(src_dirp);
        /*
            Value of dp could be NULL if src_dirp has no items. Explicit check
            of errno is required and errno has been set to 0 before readdir to
            account for any previous errors.
        */
        if(errno != 0 && dp == NULL) {
            perror("readdir");
        }

        /*
            Iterate over the directory 'src' and copy valid items in 'src' over
            to the directory in 'dest'.
        */
        while(dp != NULL) {

            if(dp->d_name[0] != '.') {
                int src_item_len = strlen(src) + strlen(dp->d_name) + 2;
                char *src_item = get_path(src, dp->d_name, src_item_len);

                if(lstat(src_item, &src_item_info) != 0) {
                    perror("lstat");
                    if(processes > 0) {
                        processes = -processes;
                    }
                }

                // Fork a new process to copy the sub-directory encountered.
                if(S_ISDIR(src_item_info.st_mode)) {
                    int pid = fork();
                    if (pid == 0) { // Recursively call copy_ftree in child.
                        exit(copy_ftree(src_item, dir_path));
                    }
                    else if (pid > 0){
                        num_children += 1;
                    }
                    else {
                        perror("Fork");
                        if(processes > 0) {
                            processes = -processes;
                        }
                    }
                }
                // Copy the file that's encountered into the directory dest.
                else if(S_ISREG(src_item_info.st_mode)) {
                    flag += copy_file(src_item, dir_path, src_item_info.st_mode,
                              src_item_info.st_size);
                    if(flag == -1 && processes > 0) {
                        processes = -processes;
                    }
                }
                free(src_item);
            }
            errno = 0;
            dp = readdir(src_dirp);
            if(errno != 0 && dp == NULL) {
                perror("readdir");
            }
        }
        closedir(src_dirp);

        int status;
        // Wait for all the children of this process to terminate.
        for(int i = 0; i < num_children; i++) {
            if (wait(&status) != -1) {
                if (WIFEXITED(status)) {
                    // Update value of processes according to whether
                    // the child exited with error.
                    if((processes * (char)WEXITSTATUS(status)) > 0) {
                        processes += WEXITSTATUS(status);
                    }
                    else if((char)WEXITSTATUS(status) < 0) {
                        processes += (-1)*WEXITSTATUS(status);
                        processes = -processes;
                    }
                    else {
                        processes = -processes;
                        processes += WEXITSTATUS(status);
                        processes = -processes;
                    }
                }
            }
        }

        // Change permissions of the copied directory to that of the
        // directory in src.
        if (chmod(dir_path, src_info.st_mode) != 0) {
            perror("Directory permissions couldn't be changed");
            if(processes > 0) {
                processes = -processes;
            }
        }
        free(dir_path);
    }

    return processes;
}


/*
    Creates a copy of the regular file src in the directory dest.
    Returns an error if src doesn't have valid permissions or if there
    exists a directory with the same name or a regular file with same name
    that doesn't have valid permissions.
*/
int copy_file(const char *src, const char *dest, mode_t perm, off_t size) {
    FILE *dest_f;
    FILE *src_f;
    char *src_name = get_name(src); // Get the name of the file.
    src_f = fopen(src, "r");
    if(src_f == NULL) { // Return error if src doesn't have read permissions.
        perror("Source file can't be opened");
        return -1;
    }
    struct stat item_info;

    int f_path_len = strlen(dest) + strlen(src_name) + 2;
    char *f_path = get_path(dest, src_name, f_path_len);

    if(lstat(f_path, &item_info) != 0) {
        if(errno != ENOENT) {
            perror("lstat - file in dest");
            free(f_path);
            fclose(src_f);
            return -1;
        }
    }

    else {
        if(S_ISDIR(item_info.st_mode)) {//Return error if the types don't match.
            printf("Type mismatch.\n");
            return -1;
        }
        else {
            chmod(f_path, 00777);
            if(size == item_info.st_size) {
                dest_f = fopen(f_path, "r");
                // Return error if the file in dest doesn't have read
                // permissions.
                if(dest_f == NULL) {
                    perror("File in destination can't be opened");
                    free(f_path);
                    fclose(src_f);
                    return -1;
                }
                // Compute hash if files have same size.
                char *src_hash = hash(src_f);
                rewind(src_f);
                char *f_path_hash = hash(dest_f);
                fclose(dest_f);

                if(strcmp(src_hash, f_path_hash) == 0) {
                    fclose(src_f);
                    if (chmod(f_path, perm) != 0) {
                        perror("File permissions couldn't be changed");
                        return -1;
                    }
                    else {
                        return 0;
                    }
                }
            }
        }
    }

    dest_f = fopen(f_path, "w");
    // Return error if file in dest doesn't have write permissions.
    if(dest_f == NULL) {
        perror("File in destination can't be written");
        fclose(src_f);
        free(f_path);
        return -1;
    }
    char buf;
    // Read data from src and write to file in dest.
    while (fread(&buf, sizeof(char), 1, src_f) != 0) {
        fwrite(&buf, sizeof(char), 1, dest_f);
    }
    fclose(dest_f);
    fclose(src_f);

    // Change permissions of the copied file to that of the file src.
    if (chmod(f_path, perm) != 0) {
        perror("File permissions couldn't be changed");
        free(f_path);
        return -1;
    }
    else {
        free(f_path);
        return 0;
    }
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


/*
    Extracts and returns the file name from path.
*/
char *get_name(const char *path) {
    const char slash = '/';
    char *name = malloc(512 * sizeof(char));
    memset(name, '\0', 512);
    if(strrchr(path, slash) == NULL) {
        strncpy(name, path, 511);
    }
    else {
        strncpy(name, strrchr(path, slash)+1, 511);
    }
    return name;
}
