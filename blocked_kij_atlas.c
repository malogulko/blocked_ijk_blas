//
// Created by Malogulko, Alexey on 13/01/2020.
//
// This code demonstrates implementation of IJK-blocked(tiled) matrix multiplication algorithm relying on cblass calls
// for an actual matrix multiplication.
//
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#ifdef __APPLE__
    // Didn't manage to build atlas on mac, will work on linux though
    #include <cblas.h>
#else
    #include <cblas-atlas.h>
#endif
#include "utils.c"

const double DGEMM_ALPLHA = 1.0; // No scaling for the product
const double DGEMM_BETA = 1.0; // Values in matrix C have to be taken into account for blocking to work

void cblas_block(double *block_a_row, double *block_b_col, double *block_c_row, int block_size) {
    cblas_dgemm(
            CblasRowMajor, // This has to be ROW due to pointer math in pointer_offset()
            CblasNoTrans,
            CblasConjTrans,
            block_size,
            block_size,
            block_size,
            DGEMM_ALPLHA,
            block_a_row,
            block_size,
            block_b_col,
            block_size,
            DGEMM_BETA,
            block_c_row,
            block_size
    );
}

/**
 * KIJ blocks iterator, e.g. if we have blocked matrices(every cell in the example is a block):
 *
 * a1 a2 a3
 * a4 a5 a6
 * a7 a8 a9
 *
 * b1 b4 b7
 * b2 b5 b8
 * b3 b6 b9
 *
 * c1 c2 c3
 * c4 c5 c6
 * c7 c8 c9
 *
 * Let's take calculating a2 for mental execution example. The pointer to a2 is (bi=0,bk=1) zero row, col 1.
 * The actual calculation based on bj:
 *
 * 1. bj=0 -> call cblas_block(a2, b2, c1, block_size), where b2 is (bk=1,bj=0) c1 is(bi=0,bj=0)
 * 2. bj=1 -> call cblas_block(a2, b5, c2, block_size), where b5 is (bk=1,bj=1) c2 is(bi=0,bj=1)
 * 3. bj=2 -> call cblas_block(a2, b8, c3, block_size), where b8 is (bk=1,bj=2) b6 is(bi=0,bj=2)
 *
 * IMPORTANT ASSUMPTION:
 * Every call to matmul(double *block_a_row, double *matrix_b_row_col, double *block_c_row, int block_size) (inner multiplication)
 * DOES NOT WIPE DATA IN *matrix_c_row matrix, instead the values from this matrix are ADDED to the result and only THEN
 * get overwritten.
 *
 * @param matrix_a_row Matrix A is both row-wise blocked with blocks themselves being row-wise organised
 * @param matrix_b_row_col Matrix B is both row-wise blocked with blocks themselves being col-wise organised
 * @param matrix_c_row Matrix C is both row-wise blocked with blocks themselves being row-wise organised
 * @param matrix_size matrix size
 * @param block_size block size
 */
void blocking_ijk(double *matrix_a_row, double *matrix_b_row_col, double *matrix_c_row, int matrix_size, int block_size) {
    for (int bk = 0; bk < matrix_size; bk += block_size) { // For matrix A - block col, for matrix B - block row
        for (int bi = 0; bi < matrix_size; bi += block_size) { // Block row in matrix C and A
            double *block_a_ptr = matrix_a_row + bi * matrix_size + bk * block_size;
            for (int bj = 0; bj < matrix_size; bj += block_size) { // Block col in matrix C / row in B
                cblas_block(block_a_ptr,
                            matrix_b_row_col + bj * block_size + bk * matrix_size,
                            matrix_c_row + bi * matrix_size + bj * block_size,
                            block_size);
            }
        }
    }
}


/**
 * The matrix representation is blocked by design, 4x4 matrix with block size 2 represented in memory as:
 *
 * 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
 *
 * Matrices A and C are stored in row-wise format(for both block and matrix):
 *
 * 01 02 05 06
 * 03 04 07 08
 * 09 10 13 14
 * 11 12 15 16
 *
 * At the same time, matrix B blocks stored in row-wise format, while block contents are column-wise:
 *
 * 01 03 05 07
 * 02 04 06 08
 * 09 11 13 15
 * 10 12 14 16
 *
 */
int main(int argc, char *argv[]) {
    int size, block_size;
    pthread_t thread_ids[2];
    struct timespec start, end;
    // Uncomment this if you want the matrices to be actually random
    //srand(time(0));
    parse_args(argc, argv, &size, &block_size);
    double *matrix_a = matrix_malloc(size);
    double *matrix_b = matrix_malloc(size);
    // Initialize info objs
    struct matrixInfo matrix_a_info = {.size = size, .mxPtr = matrix_a};
    struct matrixInfo matrix_b_info = {.size = size, .mxPtr = matrix_b};
    // Fill matrices with random values
    pthread_create(&thread_ids[0], NULL, random_matrix, &matrix_a_info);
    pthread_create(&thread_ids[1], NULL, random_matrix, &matrix_b_info);
    double *matrix_c = matrix_malloc(size); // result matrix
    for (int tn = 0; tn < 2; tn++) {
        (void) pthread_join(thread_ids[tn], 0);
    }

    //*** Commenting out the prints ***/

    //printf("Matrix A stripe:\n");
    //for (int s = 0; s < size * size; s++) {
    //    printf("%f ", *(matrix_a + s));
    //}
    //printf("\nMatrix A:\n");
    //print_matrix_blocked_rows(matrix_a, size, block_size);

    //printf("Matrix B stripe:\n");
    //for (int s = 0; s < size * size; s++) {
    //    printf("%f ", *(matrix_b + s));
    //}
    //printf("\nMatrix B:\n");
    //print_matrix_blocked_cols_in_rows(matrix_b, size, block_size);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    blocking_ijk(matrix_a, matrix_b, matrix_c, size, block_size);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);

    //*** Commenting out the prints ***/

    // printf("Matrix C stripe:\n");
    // for (int s = 0; s < size * size; s++) {
    //     printf("%f ", *(matrix_c + s));
    // }
    // printf("\nMatrix C:\n");
    // print_matrix_blocked_rows(matrix_c, size, block_size);

    uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000; // microseconds
    printf("%d;%d;%llu\n", size, block_size, delta_us);
    return 0;
}