#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

#define BLOCK_SIZE 8

/*
    Computes an 8-bit hash value for the open file pointed to by 'f'.
*/
char *hash(FILE *f) {

    char *hash_val = malloc(sizeof(char)*BLOCK_SIZE);
    memset(hash_val, '\0', BLOCK_SIZE);
    char byte;
    int index = 0;
    while (fread(&byte, sizeof(char), 1, f) != 0) {
        hash_val[index] = byte ^ hash_val[index];
        index++;
        if(index == BLOCK_SIZE){
            index = 0;
        }
    }

    return hash_val;

}
