#include <stdio.h>

// Complete these two functions according to the assignment specifications


void hash(char *hash_val, long block_size) {

    // hash_val is expected to already have all bytes initialized to '\0'
    char byte;
    int index = 0;
    while (scanf("%c", &byte) != EOF) {
        hash_val[index] = byte ^ hash_val[index];
        index++;
        if(index == block_size){
            index = 0;
        }
    }

}


int check_hash(const char *hash1, const char *hash2, long block_size) {

    int i = 0;
    for(i = 0; i < block_size; i++) {
        if(hash1[i] != hash2[i]) {
            return i;
        }
    }
    return block_size;
}
