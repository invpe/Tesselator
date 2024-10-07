// Tessie MPI
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Function to calculate Pi using the Leibniz formula for the given range
double calculate_pi(int start, int end) {
    double pi = 0.0;
    int sign = 1;

    for (int i = start; i < end; i++) {
        pi += sign * 4.0 / (2.0 * i + 1.0);
        sign = -sign; // Alternate signs
    }

    return pi;
}

void local_main(const char* arg, size_t len) {
    
    printf("Input payload: %s\n", arg);     

    // Parse the range from the decoded payload (expecting "start:end")
    int start, end;
    int parsed = sscanf(arg, "%d:%d", &start, &end);
    if (parsed != 2) {
        printf("Failed to parse range from arg. Parsed: %d\n", parsed);
        
        return;
    } 
    printf("Parsed range: %d to %d\n", start, end);

    // Calculate Pi for the given range
    double result = calculate_pi(start, end);
    printf("Pi approximation for range %d to %d: %.15f\n", start, end, result);

    // Write the result to the task_output file
    FILE *f = fopen("/spiffs/task_output", "w");
    if (f != NULL) {
        fprintf(f, "%d:%d %.15f", start,end,result);
        fclose(f);
    } else {
        printf("Failed to open task output file.\n");
    }
}
