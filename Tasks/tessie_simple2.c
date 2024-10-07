// Tessie task
// Loads a line from input
// Base64 encodes it
// Returns as output 
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>   
#include <sys/unistd.h>
#include <sys/stat.h>
#include "mbedtls/md.h"

unsigned char* base64_encode(const unsigned char* src, size_t len, size_t* out_len);

void local_main(const char* arg, size_t len) {
      
    char line[64];
    FILE *f = fopen("/spiffs/task_input","r");
    if(f != NULL)
    {
        fgets(line, sizeof(line), f);
        printf("Read: %s\n", line);
        fclose(f);    

    }
    else printf("Cant open file\n");

    size_t encoded_len;

    // Cast 'line' to 'unsigned char*' to match the expected input type
    unsigned char* pEncoded = base64_encode((unsigned char*)line, strlen(line), &encoded_len);   
 
    // Write the Base64 encoded output to the task output file
    f = fopen("/spiffs/task_output", "w");
    if(f != NULL) {
        fwrite(pEncoded, 1, encoded_len, f);
        fclose(f);    
    }

    free(pEncoded);
}
