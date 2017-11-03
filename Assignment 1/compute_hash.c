#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Hash manipulation functions in hash_functions.c
void hash(char *hash_val, long block_size);
int check_hash(const char *hash1, const char *hash2, long block_size);

#ifndef MAX_BLOCK_SIZE
    #define MAX_BLOCK_SIZE 1024
#endif

/* Converts hexstr, a string of hexadecimal digits, into hash_val, an an
 * array of char.  Each pair of digits in hexstr is converted to its
 * numeric 8-bit value and stored in an element of hash_val.
 * Preconditions:
 *    - hash_val must have enough space to store block_size elements
 *    - hexstr must be block_size * 2 characters in length
 */

void xstr_to_hash(char *hash_val, char *hexstr, int block_size) {
    for(int i = 0; i < block_size*2; i += 2) {
        char str[3];
        str[0] = hexstr[i];
        str[1] = hexstr[i + 1];
        str[2] = '\0';
        hash_val[i/2] = strtol(str, NULL, 16);
    }
}

// Print the values of hash_val in hex
void show_hash(char *hash_val, long block_size) {
    for(int i = 0; i < block_size; i++) {
        printf("%.2hhx ", hash_val[i]);
    }
    printf("\n");
}


int main(int argc, char **argv) {
    char hash_val[MAX_BLOCK_SIZE] = {'\0'};
    long block_size;

    // Verify if the user inputed correct number of arguments.
    if(argc > 3 || argc < 2) {
        printf("Usage: compute_hash BLOCK_SIZE [ COMPARISON_HASH ]\n");
        return 0;
    }

    block_size = strtol(argv[1], NULL, 10);

    // Verify if the inputed block_size is a valid one.
    if (block_size >= MAX_BLOCK_SIZE || block_size <= 0) {
        printf("The block size should be a positive integer less than %d.\n", MAX_BLOCK_SIZE);
        return 0;
    }

    // Obtain input and compute hash value.
    hash(hash_val, block_size);

    // Printing hash_val
    printf("Inp Hash : ");
    show_hash(hash_val, block_size);

    /* If a COMPARISON_HASH was provided, Check if hash_val matches
     * COMPARISON_HASH.
     */
    if (argc == 3) {

        if(strlen(argv[2]) > block_size*2) {
            printf("COMPARISON_HASH is larger than the block_size.\n");
            printf("The 2 hash values would differ.\n");
        }
        else {
            char hash_comp[block_size];
            xstr_to_hash(hash_comp, argv[2], block_size);
            int comp = check_hash(hash_val, hash_comp, block_size);

            printf("Cmp Hash : ");
            show_hash(hash_comp, block_size);

            if (comp == block_size) {
                printf("The two hash values match.\n");
            }
            else {
                printf("The first index the two hash values differ at is %d\n",
                       comp);
            }
        }

    }

    return 0;
}
