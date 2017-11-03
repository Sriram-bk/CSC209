#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "ftree.h"

#ifndef PORT
  #define PORT 30000
#endif

struct client {
    int fd;
    int curr_state;
    struct in_addr ipaddr;
    struct request rq;
    struct client *next;
};

static struct client *addclient(struct client *top, int fd, struct in_addr
                                addr);
static struct client *removeclient(struct client *top, int fd);
int handleclient(struct client *p, char *path);
int bindandlisten(unsigned short port);
int server_dir_handler(struct client *p);

int main(int argc, char **argv) {

    if(argc != 2) {
        printf("Usage:\n\t%s rcopy_server PATH_PREFIX\n", argv[0]);
        printf("\t PATH_PREFIX - The absolute path on the server that is used as the path prefix\n");
        printf("\t        for the destination in which to copy files and directories.\n");
        exit(1);
    }
    /* NOTE:  The directory PATH_PREFIX/sandbox/dest will be the directory in
     * which the source files and directories will be copied.  It therefore
	 * needs rwx permissions.  The directory PATH_PREFIX/sandbox will have
	 * write and execute permissions removed to prevent clients from trying
	 * to create files and directories above the dest directory.
     */

    // create the sandbox directory
    char path[MAXPATH];
    strncpy(path, argv[1], MAXPATH);
    strncat(path, "/", MAXPATH - strlen(path) + 1);
    strncat(path, "sandbox", MAXPATH - strlen(path) + 1);

    if(mkdir(path, 0700) == -1){
        if(errno != EEXIST) {
            fprintf(stderr, "couldn't open %s\n", path);
            perror("mkdir");
            exit(1);
        }
    }

    // create the dest directory
    strncat(path, "/", MAXPATH - strlen(path) + 1);
    strncat(path, "dest", MAXPATH - strlen(path) + 1);
    if(mkdir(path, 0700) == -1){
        if(errno != EEXIST) {
            fprintf(stderr, "couldn't open %s\n", path);
            perror("mkdir");
            exit(1);
        }
    }

    // change into the dest directory.
    chdir(path);

    // remove write and access perissions for sandbox
    // if(chmod("..", 0400) == -1) {
    //     perror("chmod");
    //     exit(1);
    // }

    /* IMPORTANT: All path operations in rcopy_server must be relative to
     * the current working directory.
     */
    rcopy_server(PORT, path);

    // Should never get here!
    fprintf(stderr, "Server reached exit point.");
    return 1;
}



void rcopy_server(unsigned short port, char* path) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    fd_set allset;
    fd_set rset;

    int i;


    int listenfd = bindandlisten(port);
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;

        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("A new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr);
        }

        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p, path);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
}

int bindandlisten(unsigned short port) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    p->curr_state = AWAITING_TYPE;
    top = p;
    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}




int handleclient(struct client *p, char *path) {

    if(p->curr_state == AWAITING_TYPE) {
        if(read(p->fd, &p->rq.type, sizeof(int)) > 0) {
            p->curr_state = AWAITING_PATH;
            return 0;
        }
        else {
            return -1;
        }
    }

    if(p->curr_state == AWAITING_PATH) {
        if(read(p->fd, p->rq.path, MAXPATH) > 0) {
            p->curr_state = AWAITING_SIZE;
            return 0;
        }
    }

    if(p->curr_state == AWAITING_SIZE) {
        if(read(p->fd, &(p->rq.size), sizeof(int)) > 0) {
            p->curr_state = AWAITING_PERM;
            return 0;
        }
    }

    if(p->curr_state == AWAITING_PERM) {
        if(read(p->fd, &(p->rq.mode), sizeof(mode_t)) > 0) {
            if(p->rq.type == REGDIR) {
                printf("dir\n");
                server_dir_handler(p);
                p->curr_state = AWAITING_TYPE;
            }
            else {
                p->curr_state = AWAITING_HASH;
                return 0;
            }
            p->curr_state = AWAITING_TYPE;
        }
        else {
            return -1;
        }
        return 0;
    }

    if(p->curr_state == AWAITING_HASH) {
        if(read(p->fd, p->rq.hash, BLOCKSIZE) > 0) {
            if(p->rq.type == TRANSFILE) {
                p->curr_state = AWAITING_DATA;
            }
            else {
                // Causes problems.
                // if(server_file_handler(p) == 1) {
                //     write(p->fd, SENDFILE, sizeof(int));
                // }
                p->curr_state = AWAITING_TYPE;
            }
            return 0;
        }
    }

    if(p->curr_state == AWAITING_DATA) {
        char buf;
        FILE *server_file = fopen(p->rq.path, "w");
        if(server_file == NULL) {
            perror("fopen");
        }
        while(read(p->fd, &buf, sizeof(char)) == 1) {
            if(fwrite(&buf, sizeof(char), 1, server_file) != 1) {
                perror("Fwrite failed");
                break;
            }
            printf("%c\n", buf);
        }
        fclose(server_file);
        int t = OK;
        write(p->fd, &t, sizeof(int));
        return -1;
    }
    return -1;

}

int server_dir_handler(struct client *p) {
    DIR *server_dir = opendir(p->rq.path);
    if(server_dir == NULL) {
        if(ENOENT == errno || ENOTDIR == errno || EACCES == errno) {
            if(ENOTDIR == errno) { // return error if the types don't match.
                printf("Type mismatch.\n");
                return -1;
            }
            // Create the directory if it doesn't exist.
            else if (ENOENT == errno) {
                if(mkdir(p->rq.path, 00777) != 0) {
                    perror("Failed to create directory");
                    return -1;
                }
            }
            // Modify permissions if dir doesn't have valid permissions.
            else {
                chmod(p->rq.path, 00777);
            }
        }
        else { // return error if opendir failed for any other reason.
            perror("opendir");
            return -1;
        }
    }
    else {
        chmod(p->rq.path, 00777);
        closedir(server_dir);
    }
    chmod(p->rq.path, p->rq.mode);
    return 0;
}


// Checks whether the file on server is different from the file on the client.
int server_file_handler(struct client *p) {

    struct stat server_file;

    if(lstat(p->rq.path, &server_file) != 0) {
        if(errno != ENOENT) {
            perror("lstat - file in dest");
            return 1;
        }
    }

    else {
        if(S_ISDIR(server_file.st_mode)) {//Return error if the types don't match.
            printf("Type mismatch.\n");
            return 1;
        }
        else {
            chmod(p->rq.path, 00777);
            if(p->rq.size == server_file.st_size) {
                FILE *server_f = fopen(p->rq.path, "r");
                // Return error if the file on the server doesn't have necessary
                // permissions.
                if(server_f == NULL) {
                    perror("File in server can't be opened");
                    return 1;
                }
                // Compute hash if files have same size.
                char *server_file_hash = malloc(sizeof(char)*BLOCKSIZE);
                hash(server_file_hash, server_f);
                fclose(server_f);

                if(strcmp(p->rq.hash, server_file_hash) == 0) {
                    return 0;
                }
            }
        }
    }
    return 0;
}
