#include <fftw3.h>
#include <stdio.h>

#define N 8  // 8x8 DCT size

// Function to perform 2D DCT-II on an 8x8 matrix
void performdct2d(double mat[8][8]) {
    // Step 1: Apply DCT to each row (1D DCT on rows)
    for (int i = 0; i < N; i++) {
        fftw_plan plan_row = fftw_plan_r2r_1d(N, mat[i], mat[i], FFTW_REDFT10, FFTW_ESTIMATE);
        fftw_execute(plan_row);
        fftw_destroy_plan(plan_row);
    }

    // Step 2: Apply DCT to each column (1D DCT on columns)
    for (int j = 0; j < N; j++) {
        double column[8];  // Temporary array to hold one column
        double column_out[8];  // Output for one column

        // Extract the column
        for (int i = 0; i < N; i++) {
            column[i] = mat[i][j];
        }

        // Apply 1D DCT to the column
        fftw_plan plan_col = fftw_plan_r2r_1d(N, column, column_out, FFTW_REDFT10, FFTW_ESTIMATE);
        fftw_execute(plan_col);
        fftw_destroy_plan(plan_col);

        // Save the result back to the output matrix
        for (int i = 0; i < N; i++) {
            mat[i][j] = column_out[i];
        }
    }
}

int main() {
    // Input 8x8 matrix
    double in[8][8] = {
        {99, 85, 75, 97, 116, 102, 73, 48}, 
        {101, 97, 83, 104, 89, 50, 38, 43}, 
        {110, 113, 106, 115, 64, 32, 34, 32}, 
        {112, 114, 117, 105, 45, 31, 57, 55}, 
        {112, 111, 112, 76, 29, 24, 58, 104}, 
        {102, 96, 100, 56, 26, 23, 48, 95}, 
        {83, 71, 80, 44, 21, 19, 16, 22}, 
        {69, 52, 67, 51, 21, 19, 10, 6}
    };

    // Perform 2D DCT on the matrix
    performdct2d(in);

    // Output result
    printf("2D DCT-II Output:\n");
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%.2f ", in[i][j]);
        }
        printf("\n");
    }

    return 0;
}
