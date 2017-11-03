#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "ftree.h"
#include "hash.h"

void print_tree(struct TreeNode *node, int depth);
/*
 * Returns the FTree rooted at the path fname.
 */
struct TreeNode *generate_ftree(const char *fname) {

    // Allocate Memory for the node on the heap.
    struct TreeNode *nodePointer = malloc(sizeof(struct TreeNode));
    struct stat info;
    struct dirent *dp;

    if(fname == NULL) {
        return NULL;
    }

    if (lstat(fname, &info) != 0) {
        perror("lstat");
        return NULL;
    }

    // Extract name of file from fname.
    const char slash = '/';
    char filename[1024] = {'\0'};
    if(strrchr(fname, slash) == NULL) {
        strncpy(filename, fname, 1023);
    }
    else {
        strncpy(filename, strrchr(fname, slash)+1, 1023);
    }
    int len = (strlen(filename));


    nodePointer->fname = malloc(sizeof(char) * (len + 1));
    strncpy(nodePointer->fname, filename, len);
    (nodePointer->fname)[len] = '\0';

    // Get permissions for file by bitwise and st_mode and mask.
    int mask = 0777;
    nodePointer->permissions = info.st_mode & mask;

    // Initialize all pointers of the TreeNode to NULL.
    nodePointer->contents = NULL;
    nodePointer->hash = NULL;
    nodePointer->next = NULL;
    int pathlen = strlen(fname);
    int namelen;

    // Check if file is a directory.
    if(S_ISDIR(info.st_mode)) {
        DIR *dirp = opendir(fname);

        // Return nodePointer if opendir fails.
    	if(dirp == NULL) {
    		perror("opendir");
            errno = 0;
    		return nodePointer;
    	}

        // Read through the directory until dp points to a valid file or
        // if dp is NULL and there is no error from readdir.
        dp = readdir(dirp);
        while((dp != NULL || errno != 0) && (dp->d_name)[0] == '.') {
            if(errno != 0) {
                perror("readdir");
                errno = 0;
            }
            dp = readdir(dirp);
        }
        if(dp != NULL) {
            // Recursively call generate_ftree till nodePointer->contents
            // has succesfully been assigned or dp reaches the end of dirp.
            do {
                namelen = strlen(dp->d_name);
                int len = namelen + pathlen + 2; // Length of absolute path.
                // Get absolute path of file d_name.
                char *path = malloc(len * sizeof(char));
                memset(path, '\0', len);
                strncpy(path, fname, pathlen);
                strncat(path, "/", 1);
                strncat(path, dp->d_name, namelen);
                nodePointer->contents = generate_ftree(path);
                free(path);
                // If nodePointer->contents was succesfully assigned NULL or
                // a TreeNode, break out of the loop.
                if(errno == 0) {
                    break;
                }
                else {
                    errno = 0;
                    dp = readdir(dirp);
                    if(errno != 0) {
                        perror("readdir");
                    }
                }
            } while(errno != 0 || dp != NULL);

            // Generate linked list of TreeNodes that represent the files
            // in the directory by recursively calling generate_ftree.
            struct TreeNode *currNodePointer = nodePointer->contents;
            dp = readdir(dirp);
            while(dp != NULL || errno != 0) {
                if(errno != 0) {
                    perror("readir");
                }
                else if((dp->d_name)[0] != '.') {
                    namelen = strlen(dp->d_name);
                    int len = namelen + pathlen + 2; // Length of absolute path.
                    // Get absolute path of file d_name.
                    char *path = malloc(len * sizeof(char));
                    memset(path, '\0', len);
                    strncpy(path, fname, pathlen);
                    strncat(path, "/", 1);
                    strncat(path, dp->d_name, namelen);
                    currNodePointer->next = generate_ftree(path);
                    free(path);
                    // Advance the pointer to the next node if
                    // currNodePointer->next was set succesfully.
                    if (errno == 0) {
                        currNodePointer = currNodePointer->next;
                    }
                }
                errno = 0;
                dp = readdir(dirp);
            }
        }
        closedir(dirp);
    }

    // Check if file is a link or a regular file.
    else if(S_ISREG(info.st_mode) || S_ISLNK(info.st_mode)) {
        FILE *f = fopen(fname, "r");
        // Assign an emtpy hash to nodePointer->hash if file can't be opened.
        if(f == NULL) {
            perror("fopen");
            nodePointer->hash = malloc(sizeof(char)*(BLOCK_SIZE));
            memset(nodePointer->hash, '\0', BLOCK_SIZE);
            errno = 0;
        }
        // Hash the contents of the file.
        else {
            nodePointer->hash = hash(f);
            fclose(f);
        }
    }

    return nodePointer;
}


/*
 * Prints the TreeNodes encountered on a preorder traversal of an FTree.
 */
void print_ftree(struct TreeNode *root) {
    static int depth = 0;
    print_tree(root, depth);
}

/*
 * Helper function for print_ftree.
 */
void print_tree(struct TreeNode *node, int depth){

    // Check if the node represents a directory.
    if(node->hash == NULL) {
        printf("%*s", depth * 2, "");
        printf("===== %s (%o) =====\n", node->fname, node->permissions);
        struct TreeNode *currNodePointer = node->contents;
        while(currNodePointer != NULL) {
            print_tree(currNodePointer, depth + 1);
            currNodePointer = currNodePointer->next;
        }

    }
    else {
        printf("%*s", depth * 2, "");
        printf("%s (%o)\n", node->fname, node->permissions);
    }
}
