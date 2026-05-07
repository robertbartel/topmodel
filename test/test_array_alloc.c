#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../include/topmodel.h"

/*
  TEST: Array Allocation Utilities
  
  Tests the memory allocation functions used throughout topmodel:
  - itwo_alloc: allocate 2D int array
  - dtwo_alloc: allocate 2D double array
  - d_alloc: allocate 1D double array
  - i_alloc: allocate 1D int array
*/

int
main(void){

    printf("\nBEGIN ARRAY ALLOCATION UNIT TEST\n**********************************\n");

    int status = 0;

    // TEST 1: d_alloc - 1D double array allocation
    printf("\nTEST 1: d_alloc (1D double array)\n");
    {
        int size = 10;
        double *arr = NULL;
        
        printf(" allocating double array of size %d...\n", size);
        d_alloc(&arr, size);
        
        assert(arr != NULL);
        printf(" allocation successful\n");
        
        // Test write/read to all elements
        printf(" testing write/read access...\n");
        for (int i = 0; i < size; i++) {
            arr[i] = (double)i * 1.5;
        }
        
        for (int i = 0; i < size; i++) {
            assert(arr[i] == (double)i * 1.5);
        }
        printf(" write/read test passed\n");
        
        free(arr);
        printf(" memory freed\n");
    }

    // TEST 2: i_alloc - 1D int array allocation
    printf("\nTEST 2: i_alloc (1D int array)\n");
    {
        int size = 15;
        int *arr = NULL;
        
        printf(" allocating int array of size %d...\n", size);
        i_alloc(&arr, size);
        
        assert(arr != NULL);
        printf(" allocation successful\n");
        
        // Test write/read to all elements
        printf(" testing write/read access...\n");
        for (int i = 0; i < size; i++) {
            arr[i] = i * 2;
        }
        
        for (int i = 0; i < size; i++) {
            assert(arr[i] == i * 2);
        }
        printf(" write/read test passed\n");
        
        free(arr);
        printf(" memory freed\n");
    }

    // TEST 3: dtwo_alloc - 2D double array allocation
    printf("\nTEST 3: dtwo_alloc (2D double array)\n");
    {
        int rows = 5;
        int cols = 8;
        double **arr = NULL;
        
        printf(" allocating 2D double array (%d x %d)...\n", rows, cols);
        dtwo_alloc(&arr, rows, cols);
        
        assert(arr != NULL);
        printf(" allocation successful\n");
        
        // Verify all rows are non-null
        printf(" verifying row pointers...\n");
        for (int i = 0; i < rows; i++) {
            assert(arr[i] != NULL);
        }
        printf(" all row pointers valid\n");
        
        // Test write/read to all elements
        printf(" testing write/read access to all elements...\n");
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                arr[i][j] = (double)(i * cols + j) * 2.5;
            }
        }
        
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                assert(arr[i][j] == (double)(i * cols + j) * 2.5);
            }
        }
        printf(" write/read test passed\n");
        
        // Free rows
        for (int i = 0; i < rows; i++) {
            free(arr[i]);
        }
        free(arr);
        printf(" memory freed\n");
    }

    // TEST 4: itwo_alloc - 2D int array allocation
    printf("\nTEST 4: itwo_alloc (2D int array)\n");
    {
        int rows = 7;
        int cols = 6;
        int **arr = NULL;
        
        printf(" allocating 2D int array (%d x %d)...\n", rows, cols);
        itwo_alloc(&arr, rows, cols);
        
        assert(arr != NULL);
        printf(" allocation successful\n");
        
        // Verify all rows are non-null
        printf(" verifying row pointers...\n");
        for (int i = 0; i < rows; i++) {
            assert(arr[i] != NULL);
        }
        printf(" all row pointers valid\n");
        
        // Test write/read to all elements
        printf(" testing write/read access to all elements...\n");
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                arr[i][j] = i * cols + j;
            }
        }
        
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                assert(arr[i][j] == i * cols + j);
            }
        }
        printf(" write/read test passed\n");
        
        // Free rows
        for (int i = 0; i < rows; i++) {
            free(arr[i]);
        }
        free(arr);
        printf(" memory freed\n");
    }

    // TEST 5: Edge case - large allocation
    printf("\nTEST 5: Edge case - large 1D allocation\n");
    {
        int size = 100000;
        double *arr = NULL;
        
        printf(" allocating large double array of size %d...\n", size);
        d_alloc(&arr, size);
        
        assert(arr != NULL);
        printf(" allocation successful\n");
        
        // Test boundary elements
        printf(" testing boundary element access...\n");
        arr[0] = 3.14;
        arr[size - 1] = 2.71;
        assert(arr[0] == 3.14);
        assert(arr[size - 1] == 2.71);
        printf(" boundary access test passed\n");
        
        free(arr);
        printf(" memory freed\n");
    }

    // TEST 6: Edge case - minimal allocation
    printf("\nTEST 6: Edge case - minimal 1D allocation\n");
    {
        int size = 1;
        double *arr = NULL;
        
        printf(" allocating single-element double array...\n");
        d_alloc(&arr, size);
        
        assert(arr != NULL);
        printf(" allocation successful\n");
        
        arr[0] = 42.0;
        assert(arr[0] == 42.0);
        printf(" single element access test passed\n");
        
        free(arr);
        printf(" memory freed\n");
    }

    printf("\n**********************************\nEND ARRAY ALLOCATION UNIT TEST\n\n");
    return 0;
}
