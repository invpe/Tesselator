/*
    The Tesselator task applies Delayed Space Coordinates (DSC) transformation to a dataset, 
    projecting the resulting points into a 2D grid and calculating entropy.
    It ties to my speculative researches i always like doing.

    The steps are as follows:
    1. The dataset is read in chunks, and for each chunk, a DSC transformation maps the data points into 3D space.
    2. The 3D points (x, y, z) are normalized to the range [0, 1] based on the minimum and maximum values of the dataset.
    3. These points are then projected onto a 2D grid of size GRID_SIZE x GRID_SIZE (e.g., 10x10), where the x and y coordinates are used for binning, while the z-axis (depth) is ignored.
    4. The Y-axis is inverted during binning to follow the top-down coordinate system, with higher Y-values at the top of the grid.
    5. The grid represents a 2D projection, where each cell counts the number of points that fall into it.
    6. The final bin counts are used to estimate clustering or randomness in the dataset using entropy calculation.

    The mapping logic:
    - The x and y coordinates are scaled from [-1, 1] to fit the grid, dynamically adjusting based on GRID_SIZE.
    - Each grid cell accumulates the number of points within its region, ignoring the z-depth (projection).
    - The Y-axis is inverted to align with typical top-left origin visualization.
    - For example, the top-left bin represents (-GRID_SIZE/2, GRID_SIZE/2) in x, y space, and the bottom-right bin represents (GRID_SIZE/2, -GRID_SIZE/2).

    Entropy Calculation:
    - Entropy is calculated to analyze the randomness or clustering of the dataset.
    - A higher entropy value indicates more randomness, while a lower entropy suggests clustering.
    - The entropy result can help determine whether a dataset is worth further visual inspection or if it exhibits random patterns.

    The main purpose of this task is to detect and analyze clustering in the dataset by visualizing the density of points.
    
    The visualization tool that comes as bundle for this task uses OpenGL for rendering the DSC data in 3D space
    Important: no more than 10k of inputs per dataset is suggested
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h> 
#include <math.h>

#define GRID_SIZE 10  // Defines a 10x10 grid for x and y axes

// Global bins for 2D projection (x, y)
int bins[GRID_SIZE][GRID_SIZE] = {0}; 
int iTotalPointsCount = 0;

struct tPosition {
    double x;
    double y;
    double z;
};
  
double fast_log2(double x) {
    int exp;
    double frac = frexp(x, &exp); // Separate the exponent and fraction
    return exp + (frac - 1.0); // Approximation
}

void calculate_2D_bin(struct tPosition pos) {
    // Scale pos.x and pos.y from [-1, 1] to [0, GRID_SIZE - 1] range for binning
    double scale_factor = (double)(GRID_SIZE / 2);
    double grid_x = pos.x * scale_factor;  // Scale from [-1, 1] to [-GRID_SIZE/2, GRID_SIZE/2]
    double grid_y = pos.y * scale_factor;  // Scale from [-1, 1] to [-GRID_SIZE/2, GRID_SIZE/2]

    // Calculate bin index (scale to the range [0, GRID_SIZE-1] from [-GRID_SIZE/2, GRID_SIZE/2])
    int x_bin = (int)((grid_x + (GRID_SIZE / 2.0)));  // Offset by half of GRID_SIZE to shift into [0, GRID_SIZE-1]
    int y_bin = (int)((-grid_y + (GRID_SIZE / 2.0)));  // Invert Y-axis for top-down grid orientation

    // Ensure bin indices are within valid range [0, GRID_SIZE-1]
    if (x_bin >= GRID_SIZE) x_bin = GRID_SIZE - 1;
    if (y_bin >= GRID_SIZE) y_bin = GRID_SIZE - 1;
    if (x_bin < 0) x_bin = 0;
    if (y_bin < 0) y_bin = 0;

    // Increment the count in the corresponding 2D bin
    bins[x_bin][y_bin]++;
}

// Function to process a chunk of data, calculate DSC, and count points in 2D bins
int ProcessChunkAnd2DBin(double* buffer, size_t buffer_size, double iMinValue, double iMaxValue,FILE *fp) {
    struct tPosition pos;

    // Calculate DSC and normalize for all but the first 3 elements (which don't have full history)
    for (size_t a = 3; a < buffer_size; a++) {
        pos.x = buffer[a] - buffer[a - 1];
        pos.y = buffer[a - 1] - buffer[a - 2];
        pos.z = buffer[a - 2] - buffer[a - 3];

        // Normalize to the range [0, 1]
        pos.x = (pos.x - iMinValue) / (iMaxValue - iMinValue);
        pos.y = (pos.y - iMinValue) / (iMaxValue - iMinValue);
        pos.z = (pos.z - iMinValue) / (iMaxValue - iMinValue);

        //
        fprintf(fp, "x=%lf, y=%lf, z=%lf\n", pos.x, pos.y, pos.z);

        // Calculate the 2D bin (ignoring z-axis)
        calculate_2D_bin(pos);

        iTotalPointsCount++;
    }

    return 0;
}

// Function to load the data file in chunks, calculate DSC, and bin points in 2D
int LoadFileInChunksAnd2DBin(const char* arg,const char* filename, const char* output_filename, size_t chunk_size) 
{
 
    FILE* input_file = fopen(filename, "r");
    if (!input_file) {
        printf("Failed to open the file.\n");
        return 1;
    }

    FILE* output_file = fopen(output_filename, "w");
    if (!output_file) {
        printf("Failed to open the output file.\n");
        fclose(input_file);
        return 1;
    }

    double number;
    double iMaxValue = -DBL_MAX;
    double iMinValue = DBL_MAX;

    size_t buffer_size = chunk_size + 3;  // 3 extra slots for DSC history
    double* buffer = (double*)malloc(buffer_size * sizeof(double));
    if (!buffer) {
        printf("Memory allocation failed.\n");
        fclose(input_file);
        fclose(output_file);
        return 1;
    }

    size_t count = 0;
    size_t chunk_index = 0;

    // Read data in chunks and process
    while (fscanf(input_file, "%lf", &number) == 1) {
        if (number < iMinValue) iMinValue = number;
        if (number > iMaxValue) iMaxValue = number;

        buffer[count++] = number;

        // When the buffer is full, process it
        if (count == buffer_size) {
            printf("Processing chunk %zu\n", chunk_index);

            ProcessChunkAnd2DBin(buffer, buffer_size, iMinValue, iMaxValue,output_file);
            chunk_index++;

            // Move the last 3 elements to the front of the buffer for the next chunk
            buffer[0] = buffer[buffer_size - 3];
            buffer[1] = buffer[buffer_size - 2];
            buffer[2] = buffer[buffer_size - 1];

            count = 3;  // Reset count to continue filling the buffer
        }
    }

    // Process the remaining data if the file didn't exactly fill the buffer
    if (count > 3) {
        ProcessChunkAnd2DBin(buffer, count, iMinValue, iMaxValue,output_file);
    }

    double entropy = 0.0;

    // Print out the 2D bin counts to the output file with corresponding [-5,5] labeling
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            int x_label = i - (GRID_SIZE / 2);  // Convert bin index to the range [-5, 5]
            int y_label = j - (GRID_SIZE / 2);  // Convert bin index to the range [-5, 5]
            fprintf(output_file, "# Bin (%d, %d): Points = %d\n", x_label, y_label, bins[i][j]);

            // Calculate entropy 
            if (bins[i][j] > 0) {
                double p = (double)bins[i][j] / iTotalPointsCount;
                entropy -= p * fast_log2(p);
            }            
        }
    }
    
    fprintf(output_file, "# Entropy = %f\n",entropy);
    fprintf(output_file, "# Argument = %s\n", arg);

    // Clean up
    free(buffer);
    fclose(input_file);
    fclose(output_file);

    return 0;
}

void local_main(const char* arg, size_t len) {
    printf("Starting task\n");

    // Process 50 chunks of raw data at once (mem safe)
    LoadFileInChunksAnd2DBin(arg,"/spiffs/task_input", "/spiffs/task_output", 100);  

    printf("Job done\n");
}
