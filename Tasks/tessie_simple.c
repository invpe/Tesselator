// Tessie simple task
// Demonstrates argument
// Loading input file if given
// Stores results in output file
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>   
#include <sys/unistd.h>
#include <sys/stat.h>

void local_main(const char* arg, size_t len) {
     
    printf("Welcome to task\n"); 
    printf("Input argument    : %s\n",arg);
    printf("Input argument len: %i\n", len);    
    printf("Opening payload file\n");

    char line[64];
    FILE *f = fopen("/spiffs/task_input","r");
    if(f != NULL)
    {
        fgets(line, sizeof(line), f);
        printf("Read: %s\n", line);
        fclose(f);    


    }
    else printf("Payload file not found\n");

    // Simply write output
    printf("Storing to output\n");

    f=fopen("/spiffs/task_output","w");
    fprintf(f,"This is output\n");
    fprintf(f,"Argument given: %s\n", arg);    
    fclose(f);

    // Complete
    printf("Task done\n");

}
