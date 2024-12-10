#include "ffwt.h"

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