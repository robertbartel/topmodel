#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../include/topmodel.h"

/* Forward declaration of shift_Q since it's not in header */
extern void shift_Q(double* Q, int num_ordinates);

/*
  TEST: shift_Q Function
  
  Tests the array shifting utility used to manage discharge (Q) arrays:
  - shift_Q: shifts array left by one element, sets last element to 0.0
  
  This function is critical in the BMI update loop for managing discharge time series.
*/

int
main(void){

    printf("\nBEGIN SHIFT_Q UNIT TEST\n**********************\n");

    // TEST 1: Basic shift operation
    printf("\nTEST 1: Basic shift operation\n");
    {
        int size = 5;
        double Q[size];
        
        printf(" initializing Q array with values [1.0, 2.0, 3.0, 4.0, 5.0]...\n");
        Q[0] = 1.0;
        Q[1] = 2.0;
        Q[2] = 3.0;
        Q[3] = 4.0;
        Q[4] = 5.0;
        
        printf(" before shift: ");
        for (int i = 0; i < size; i++) printf("%f ", Q[i]);
        printf("\n");
        
        // Shift array: num_ordinates is the number of elements to shift/manage
        // In shift_Q implementation, it uses Q[1] through Q[num_ordinates]
        // and sets Q[num_ordinates] = 0.0
        shift_Q(Q, size - 1);  // num_ordinates = size - 1
        
        printf(" after shift:  ");
        for (int i = 0; i < size; i++) printf("%f ", Q[i]);
        printf("\n");
        
        // After shift: [2.0, 3.0, 4.0, 5.0, 0.0]
        assert(Q[0] == 2.0);
        assert(Q[1] == 3.0);
        assert(Q[2] == 4.0);
        assert(Q[3] == 5.0);
        assert(Q[4] == 0.0);
        
        printf(" shift test passed\n");
    }

    // TEST 2: Shift with different size
    printf("\nTEST 2: Shift with size 10\n");
    {
        int size = 10;
        double Q[size];
        
        printf(" initializing Q array with sequential values...\n");
        for (int i = 0; i < size; i++) {
            Q[i] = (double)(i + 1);  // 1.0 through 10.0
        }
        
        printf(" before shift: ");
        for (int i = 0; i < size; i++) printf("%.0f ", Q[i]);
        printf("\n");
        
        shift_Q(Q, size - 1);
        
        printf(" after shift:  ");
        for (int i = 0; i < size; i++) printf("%.0f ", Q[i]);
        printf("\n");
        
        // Verify the shift: [2, 3, 4, 5, 6, 7, 8, 9, 10, 0]
        for (int i = 0; i < size - 1; i++) {
            assert(Q[i] == (double)(i + 2));
        }
        assert(Q[size - 1] == 0.0);
        
        printf(" shift test passed\n");
    }

    // TEST 3: Multiple sequential shifts
    printf("\nTEST 3: Multiple sequential shifts\n");
    {
        int size = 6;
        double Q[size];
        
        printf(" initializing Q array...\n");
        for (int i = 0; i < size; i++) {
            Q[i] = (double)(i + 1);  // 1, 2, 3, 4, 5, 6
        }
        
        printf(" initial state: ");
        for (int i = 0; i < size; i++) printf("%.0f ", Q[i]);
        printf("\n");
        
        // First shift: [2, 3, 4, 5, 6, 0]
        shift_Q(Q, size - 1);
        printf(" after shift 1: ");
        for (int i = 0; i < size; i++) printf("%.0f ", Q[i]);
        printf("\n");
        assert(Q[0] == 2.0 && Q[size - 1] == 0.0);
        
        // Second shift: [3, 4, 5, 6, 0, 0]
        shift_Q(Q, size - 1);
        printf(" after shift 2: ");
        for (int i = 0; i < size; i++) printf("%.0f ", Q[i]);
        printf("\n");
        assert(Q[0] == 3.0 && Q[size - 1] == 0.0);
        
        // Third shift: [4, 5, 6, 0, 0, 0]
        shift_Q(Q, size - 1);
        printf(" after shift 3: ");
        for (int i = 0; i < size; i++) printf("%.0f ", Q[i]);
        printf("\n");
        assert(Q[0] == 4.0 && Q[size - 1] == 0.0);
        
        printf(" multiple shift test passed\n");
    }

    // TEST 4: Shift with floating point values
    printf("\nTEST 4: Shift with floating point values\n");
    {
        int size = 5;
        double Q[size];
        
        printf(" initializing Q array with floating point values...\n");
        Q[0] = 1.234;
        Q[1] = 2.567;
        Q[2] = 3.891;
        Q[3] = 4.123;
        Q[4] = 5.456;
        
        printf(" before shift: ");
        for (int i = 0; i < size; i++) printf("%f ", Q[i]);
        printf("\n");
        
        shift_Q(Q, size - 1);
        
        printf(" after shift:  ");
        for (int i = 0; i < size; i++) printf("%f ", Q[i]);
        printf("\n");
        
        // Verify with floating point comparison (allow small tolerance)
        double tolerance = 1e-10;
        assert(Q[0] - 2.567 < tolerance && 2.567 - Q[0] < tolerance);
        assert(Q[1] - 3.891 < tolerance && 3.891 - Q[1] < tolerance);
        assert(Q[2] - 4.123 < tolerance && 4.123 - Q[2] < tolerance);
        assert(Q[3] - 5.456 < tolerance && 5.456 - Q[3] < tolerance);
        assert(Q[4] == 0.0);
        
        printf(" floating point shift test passed\n");
    }

    // TEST 5: Shift with negative values
    printf("\nTEST 5: Shift with negative values\n");
    {
        int size = 4;
        double Q[size];
        
        printf(" initializing Q array with mixed positive/negative values...\n");
        Q[0] = -5.0;
        Q[1] = 3.5;
        Q[2] = -2.1;
        Q[3] = 7.9;
        
        printf(" before shift: ");
        for (int i = 0; i < size; i++) printf("%f ", Q[i]);
        printf("\n");
        
        shift_Q(Q, size - 1);
        
        printf(" after shift:  ");
        for (int i = 0; i < size; i++) printf("%f ", Q[i]);
        printf("\n");
        
        assert(Q[0] == 3.5);
        assert(Q[1] == -2.1);
        assert(Q[2] == 7.9);
        assert(Q[3] == 0.0);
        
        printf(" negative value shift test passed\n");
    }

    // TEST 6: Edge case - minimal array
    printf("\nTEST 6: Edge case - minimal array (size 2)\n");
    {
        int size = 2;
        double Q[size];
        
        printf(" initializing Q array...\n");
        Q[0] = 10.0;
        Q[1] = 20.0;
        
        printf(" before shift: %f %f\n", Q[0], Q[1]);
        
        shift_Q(Q, size - 1);
        
        printf(" after shift:  %f %f\n", Q[0], Q[1]);
        
        assert(Q[0] == 20.0);
        assert(Q[1] == 0.0);
        
        printf(" minimal array shift test passed\n");
    }

    // TEST 7: Large array shift
    printf("\nTEST 7: Large array shift (size 100)\n");
    {
        int size = 100;
        double *Q = (double*)malloc(size * sizeof(double));
        
        printf(" initializing large Q array...\n");
        for (int i = 0; i < size; i++) {
            Q[i] = (double)(i + 1);
        }
        
        // Sample before
        printf(" before shift: Q[0]=%.0f, Q[50]=%.0f, Q[99]=%.0f\n", Q[0], Q[50], Q[99]);
        
        shift_Q(Q, size - 1);
        
        // Sample after
        printf(" after shift:  Q[0]=%.0f, Q[50]=%.0f, Q[99]=%.0f\n", Q[0], Q[50], Q[99]);
        
        // Verify shift worked correctly
        assert(Q[0] == 2.0);
        assert(Q[50] == 52.0);
        assert(Q[99] == 0.0);
        
        printf(" large array shift test passed\n");
        
        free(Q);
    }

    printf("\n**********************\nEND SHIFT_Q UNIT TEST\n\n");
    return 0;
}
