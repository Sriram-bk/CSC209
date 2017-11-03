#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include "ftree.h"
#include "hash.h"


#ifndef PORT
  #define PORT 58915
#endif

int traverse_dir(char* server_path, char* client_path, char* host, int soc,
                 unsigned short port);

int client_file_handler(char* server_path, char* client_path, char* host,
                        int soc, unsigned short port);

int transfer_file(char* server_path, char* client_path, char* host, char*
                  file_hash, unsigned short port);

int main(int argc, char **argv) {
    /* Note: In most cases, you'll want HOST to be localhost or 127.0.0.1, so
     * you can test on your local machine.*/
    if (argc != 3) {
        printf("Usage:\n\trcopy_client SRC HOST\n");
        printf("\t SRC - The file or directory to copy to the server\n");
        printf("\t HOST - The hostname of the server");
        return 1;
    }

    if (rcopy_client(argv[1], argv[2], PORT) != 0) {
        printf("Errors encountered during copy\n");
        return 1;
    } else {
        printf("Copy completed successfully\n");
        return 0;
    }
}


int rcopy_client(char *source, char *host, unsigned short port) {

    struct sockaddr_in peer;
    int soc;

    if ((soc = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("randclient: socket");
      exit(1);
    }

    peer.sin_family = AF_INET;
    peer.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &peer.sin_addr) < 1) {
      perror("randclient: inet_pton");
      close(soc);
      exit(1);
    }

    if (connect(soc, (struct sockaddr *)&peer, sizeof(peer)) == -1) {
      perror("randclient: connect");
      exit(1);
    }

    struct stat src_info;
    char *src_name = get_name(source);
    if ((lstat(source, &src_info) != 0)) {
        perror("lstat");
        return -1;
    }

    if (S_ISREG(src_info.st_mode)) {
        client_file_handler(src_name, source, host, soc, port);
    }

    else if(S_ISDIR(src_info.st_mode)) {
        traverse_dir(src_name, source, host, soc, port);
    }

    free(src_name);
    close(soc);

    return 0;
}

int traverse_dir(char* server_path, char* client_path, char* host, int soc,
                 unsigned short port) {

    struct stat dir_info, item_info;
    struct dirent* dp;
    int status;
    int type = REGDIR;

    if ((lstat(client_path, &dir_info) != 0)) {
        perror("lstat");
        return -1;
    }


    //Send the server details of the directory.
    write(soc, &type, sizeof(int));
    write(soc, server_path, MAXPATH);
    write(soc, &dir_info.st_size, sizeof(int));
    write(soc, &dir_info.st_mode, sizeof(mode_t));
    //No Hash for directories.

    // // Wait for message back from server.
    // while(read(soc, &status, sizeof(int)) == 0) {
    // }
    status = 0;

    // Check if there was a type mismatch between client and server.
    if(status != ERROR) {
        DIR *dirp = opendir(client_path);
        if(dirp == NULL) {
            perror("Directory doesn't exist");
            return -1;
        }
        errno = 0;
        dp = readdir(dirp);
        /*
            Value of dp could be NULL if dirp has no items. Explicit check
            of errno is required and errno has been set to 0 before readdir to
            account for any previous errors.
        */
        if(errno != 0 && dp == NULL) {
            perror("readdir");
        }

        while(dp != NULL) {

            if(dp->d_name[0] != '.') {

                int item_path_len = strlen(client_path) + strlen(dp->d_name) +
                                    2;
                char *item_path = get_path(client_path, dp->d_name,
                                           item_path_len);

                if ((lstat(item_path, &item_info) != 0)) {
                    perror("lstat");
                    return -1;
                }

                int path_len = strlen(server_path) + strlen(dp->d_name) +
                                    2;
                char *path = get_path(server_path, dp->d_name, path_len);

                if(S_ISDIR(item_info.st_mode)) {
                    printf("one dir\n");
                    traverse_dir(path, item_path, host, soc, port);
                }

                if(S_ISREG(item_info.st_mode)) {
                    client_file_handler(path, item_path, host, soc, port);
                }
            }
            dp = readdir(dirp);

        }

    }
    else { //Type mismatch between client and server. Return error.
        return -1;
    }

    //Should never get here.
    return -1;
}



int client_file_handler(char* server_path, char* client_path, char* host,
                        int soc, unsigned short port) {
    struct stat file_info;
    int type = REGFILE;
    int status = 0;

    if ((lstat(client_path, &file_info) != 0)) {
        perror("lstat");
        return -1;
    }

    //Compute hash for the file.
    char *file_hash = malloc(sizeof(char)*BLOCKSIZE);
    // Make sure we have proper permissions to copy file.
    chmod(client_path, 00777);
    FILE *f = fopen(client_path,"r");
    if (f == NULL) {
        perror("File couldn't be opened");
        return -1;
    }
    hash(file_hash, f);
    fclose(f);
    //Reset permissions of file in server.
    chmod(client_path, file_info.st_mode);

    //Send the server details of the file.
    write(soc, &type, sizeof(int));
    write(soc, server_path, MAXPATH);
    write(soc, &file_info.st_size, sizeof(int));
    write(soc, &file_info.st_mode, sizeof(mode_t));
    write(soc, file_hash, BLOCKSIZE);

    // Wait for message back from server. Don't get how to wait for message
    // from server.
    // if (read(soc, &status, sizeof(int)) == 0) {
    // }

    status = 1;
    //Do nothing if the files in client and server are the same.
    if (status == OK) {
        return 0;
    }

    //Read data from file and write to soc.
    else if (status == SENDFILE) {
        int pid = fork();
        if (pid == 0) { //Child handles file transfer.
            transfer_file(server_path, client_path, host, file_hash, port);
        }
        else if (pid > 0){
            int st;
            if (wait(&st) != -1) {
                if (WIFEXITED(st)) {
                    printf("child exited\n");
                    return 0;
                }
            }
        }
        else {
            perror("Fork");
            return -1;
        }
    }

    //Return error if there's type mismatch between client and server.
    else if (status == ERROR) {
        return -1;
    }

    //Should never get here.
    else {
        printf("Invalid status ID.\n");
        return -1;
    }
    return -1;
}

// Send data of the requested file through a new socket connection.
int transfer_file(char* server_path, char* client_path, char* host, char* file_hash, unsigned short port) {

    struct stat file_info;
    int type = TRANSFILE;
    int status = 0;
    struct sockaddr_in peer;
    int trans_soc;

    if ((trans_soc = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("randclient: socket");
      exit(1);
    }

    peer.sin_family = AF_INET;
    peer.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &peer.sin_addr) < 1) {
      perror("randclient: inet_pton");
      close(trans_soc);
      exit(1);
    }

    if (connect(trans_soc, (struct sockaddr *)&peer, sizeof(peer)) == -1) {
      perror("randclient: connect");
      exit(1);
    }

    if ((lstat(client_path, &file_info) != 0)) {
      perror("lstat");
      return -1;
    }


    //Send the server details of the file to be transfered.
    write(trans_soc, &type, sizeof(int));
    write(trans_soc, server_path, MAXPATH);
    write(trans_soc, &file_info.st_size, sizeof(int));
    write(trans_soc, &file_info.st_mode, sizeof(mode_t));
    write(trans_soc, file_hash, BLOCKSIZE);

    //Make sure we have proper permissions to copy file.
    chmod(client_path, 00777);
    FILE *f = fopen(client_path,"r");
    if (f == NULL) {
        perror("File couldn't be opened");
        return -1;
    }
    char buf;
    while (fread(&buf, sizeof(char), 1, f) != 0) {
        write(trans_soc, &buf, sizeof(char));
        printf("%c\n", buf);
    }
    if(fclose(f) == EOF) {
        perror("File close");
    }

    // //Wait for message back from server.
    // if(read(trans_soc, &status, sizeof(int)) == 0) {
    // }

    //if transfer completed successfully close socket and exit.
    if(status == OK) {
        close(trans_soc);
        exit(0);
    }
    else if(status == ERROR) {
        printf("Transfer of %s encountered an error.", server_path);
        close(trans_soc);
        exit(1);
    }

    //Should never get here.
    else {
        printf("Invalid status ID.\n");
        return -1;
    }

    return -1;

}
