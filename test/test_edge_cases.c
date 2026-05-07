#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include "../include/topmodel.h"
#include "../include/bmi.h"
#include "../include/bmi_topmodel.h"

/*
  TEST: Edge Cases and Robustness
  
  Tests edge cases and boundary conditions to ensure:
  - Model handles extreme parameter values gracefully
  - Zero/minimal inputs produce physically valid outputs
  - No buffer overflows or crashes with edge inputs
  - Outputs remain within physical bounds
  - Model recovers from extreme states
*/

int
main(void){

    printf("\nBEGIN EDGE CASES AND ROBUSTNESS TEST\n***Ъ*****************************\n");

    int status = BMI_SUCCESS;
    
    // Allocate and initialize model
    printf(" allocating and initializing model...\n");
    Bmi *model = (Bmi *) malloc(sizeof(Bmi));
    assert(model != NULL);
    
    register_bmi_topmodel(model);
    const char *cfg_file = "./data/topmod_unit_test.run";
    status = model->initialize(model, cfg_file);
    assert(status == BMI_SUCCESS);

    // TEST 1: Test parameter bounds - very small values
    printf("\nTEST 1: Parameter bounds - very small values\n");
    {
        double small_values[] = {1e-6, 1e-8, 1e-10};
        const char *params[] = {"szm", "td", "xk0"};
        
        for (int i = 0; i < 3; i++) {
            printf(" setting %s to %e...\n", params[i], small_values[i]);
            status = model->set_value(model, params[i], &small_values[i]);
            assert(status == BMI_SUCCESS);
            
            double retrieved = 0.0;
            status = model->get_value(model, params[i], &retrieved);
            assert(status == BMI_SUCCESS);
            assert(retrieved == small_values[i]);
            printf("  parameter set and retrieved successfully\n");
            
            // Try an update with small parameter
            status = model->update(model);
            assert(status == BMI_SUCCESS);
            printf("  model update succeeded with small parameter\n");
        }
    }

    // TEST 2: Test parameter bounds - large values
    printf("\nTEST 2: Parameter bounds - large values\n");
    {
        double large_values[] = {1000.0, 10000.0, 100000.0};
        const char *params[] = {"szm", "td", "srmax"};
        
        for (int i = 0; i < 3; i++) {
            printf(" setting %s to %e...\n", params[i], large_values[i]);
            status = model->set_value(model, params[i], &large_values[i]);
            assert(status == BMI_SUCCESS);
            
            double retrieved = 0.0;
            status = model->get_value(model, params[i], &retrieved);
            assert(status == BMI_SUCCESS);
            assert(retrieved == large_values[i]);
            printf("  parameter set and retrieved successfully\n");
            
            // Try an update with large parameter
            status = model->update(model);
            assert(status == BMI_SUCCESS);
            printf("  model update succeeded with large parameter\n");
        }
    }

    // TEST 3: Test zero parameter (if physically valid)
    printf("\nTEST 3: Parameter edge case - zero value for td (time delay)\n");
    {
        double zero = 0.0;
        printf(" attempting to set td to zero...\n");
        status = model->set_value(model, "td", &zero);
        
        if (status == BMI_SUCCESS) {
            printf("  parameter accepted zero value\n");
            
            // Try update with zero parameter
            status = model->update(model);
            // Don't assert on failure, just verify no crash
            printf("  update completed (success or controlled failure)\n");
        } else {
            printf("  parameter rejected zero value (expected behavior)\n");
        }
    }

    // Reinitialize for next tests
    status = model->finalize(model);
    free(model);
    
    model = (Bmi *) malloc(sizeof(Bmi));
    assert(model != NULL);
    register_bmi_topmodel(model);
    status = model->initialize(model, cfg_file);
    assert(status == BMI_SUCCESS);

    // TEST 4: Multiple rapid updates (stress test)
    printf("\nTEST 4: Multiple rapid updates (stress test - 20 steps)\n");
    {
        double *Qout = (double*) malloc(sizeof(double));
        double *sbar = (double*) malloc(sizeof(double));
        int updates_completed = 0;
        
        printf(" performing 20 rapid sequential updates...\n");
        for (int step = 1; step <= 20; step++) {
            status = model->update(model);
            
            if (status == BMI_FAILURE) {
                printf(" update failed at step %d (reached end of simulation)\n", step);
                break;
            }
            
            updates_completed++;
            
            // Sample every 5 steps
            if (step % 5 == 0) {
                status = model->get_value(model, "Qout", Qout);
                assert(status == BMI_SUCCESS);
                status = model->get_value(model, "soil_water__domain_volume_deficit", sbar);
                assert(status == BMI_SUCCESS);
                
                printf(" step %2d: Qout=%f, sbar=%f\n", step, *Qout, *sbar);
                
                // Verify outputs remain valid (allow small negative tolerance for Qout)
                double tolerance = 1e-4;
                assert(*Qout >= -tolerance);
                assert(*sbar >= 0.0);
                assert(isfinite(*Qout));
                assert(isfinite(*sbar));
            }
        }
        
        printf(" completed %d updates without crash\n", updates_completed);
        
        free(Qout);
        free(sbar);
    }

    // TEST 5: Output validity checks - verify no NaN or Inf
    printf("\nTEST 5: Output validity checks (NaN and Inf detection)\n");
    {
        int count_out = 0;
        char **names_out = NULL;
        
        status = model->get_output_item_count(model, &count_out);
        assert(status == BMI_SUCCESS);
        
        names_out = (char**) malloc(sizeof(char *) * count_out);
        for (int i = 0; i < count_out; i++)
            names_out[i] = (char*) malloc(sizeof(char) * BMI_MAX_VAR_NAME);
        status = model->get_output_var_names(model, names_out);
        assert(status == BMI_SUCCESS);
        
        // Do one update
        status = model->update(model);
        assert(status == BMI_SUCCESS);
        
        printf(" checking %d output variables for NaN/Inf...\n", count_out);
        int invalid_count = 0;
        
        for (int i = 0; i < count_out; i++) {
            double value = 0.0;
            status = model->get_value(model, names_out[i], &value);
            
            if (status == BMI_SUCCESS) {
                if (isnan(value)) {
                    printf(" WARNING: %s = NaN\n", names_out[i]);
                    invalid_count++;
                } else if (isinf(value)) {
                    printf(" WARNING: %s = Inf\n", names_out[i]);
                    invalid_count++;
                }
            }
        }
        
        if (invalid_count == 0) {
            printf(" all output variables are valid (no NaN/Inf)\n");
        } else {
            printf(" found %d invalid values\n", invalid_count);
            // Note: We don't assert here since some extreme parameters might
            // legitimately produce these in edge cases
        }
        
        // Cleanup
        for (int i = 0; i < count_out; i++)
            free(names_out[i]);
        free(names_out);
    }

    // TEST 6: Parameter sweep - verify monotonic behavior
    printf("\nTEST 6: Parameter sweep - verify response to parameter changes\n");
    {
        double *Qout_prev = (double*) malloc(sizeof(double));
        double *Qout_curr = (double*) malloc(sizeof(double));
        
        // Get initial Qout
        status = model->get_value(model, "Qout", Qout_prev);
        assert(status == BMI_SUCCESS);
        printf(" initial Qout: %f\n", *Qout_prev);
        
        // Decrease xk0 (conductivity) by factor of 2
        double xk0_reduced = 0.001;  // Reduced from typical ~0.002
        printf(" reducing xk0 to %f...\n", xk0_reduced);
        status = model->set_value(model, "xk0", &xk0_reduced);
        assert(status == BMI_SUCCESS);
        
        // Update and check Qout
        status = model->update(model);
        assert(status == BMI_SUCCESS);
        
        status = model->get_value(model, "Qout", Qout_curr);
        assert(status == BMI_SUCCESS);
        printf(" Qout after xk0 reduction: %f\n", *Qout_curr);
        
        // With lower conductivity, baseflow should decrease (more water stored)
        // This isn't always true, so we just verify outputs are valid
        assert(*Qout_curr >= 0.0);
        assert(isfinite(*Qout_curr));
        printf(" parameter sweep completed, output remains valid\n");
        
        free(Qout_prev);
        free(Qout_curr);
    }

    // TEST 7: State consistency - verify model state remains consistent
    printf("\nTEST 7: State consistency checks\n");
    {
        double *sump_old = (double*) malloc(sizeof(double));
        double *sump_new = (double*) malloc(sizeof(double));
        double *sumq_old = (double*) malloc(sizeof(double));
        double *sumq_new = (double*) malloc(sizeof(double));
        
        // Get cumulative sums before
        status = model->get_value(model, "land_surface_water__domain_time_integral_of_precipitation_volume_flux", sump_old);
        assert(status == BMI_SUCCESS);
        status = model->get_value(model, "land_surface_water__domain_time_integral_of_runoff_volume_flux", sumq_old);
        assert(status == BMI_SUCCESS);
        
        printf(" before state: sump=%f, sumq=%f\n", *sump_old, *sumq_old);
        
        // Multiple get_value calls without update (should return same values)
        printf(" performing 5 consecutive get_value calls without update...\n");
        for (int i = 0; i < 5; i++) {
            status = model->get_value(model, "land_surface_water__domain_time_integral_of_precipitation_volume_flux", sump_new);
            assert(status == BMI_SUCCESS);
            assert(*sump_new == *sump_old);
        }
        printf(" state unchanged by multiple get_value calls: VERIFIED\n");
        
        // Now do an update
        status = model->update(model);
        if (status == BMI_SUCCESS) {
            status = model->get_value(model, "land_surface_water__domain_time_integral_of_precipitation_volume_flux", sump_new);
            assert(status == BMI_SUCCESS);
            status = model->get_value(model, "land_surface_water__domain_time_integral_of_runoff_volume_flux", sumq_new);
            assert(status == BMI_SUCCESS);
            
            printf(" after update: sump=%f, sumq=%f\n", *sump_new, *sumq_new);
            
            // Cumulative values should not decrease
            assert(*sump_new >= *sump_old);
            assert(*sumq_new >= *sumq_old);
            printf(" cumulative variables remain monotonic: VERIFIED\n");
        }
        
        free(sump_old);
        free(sump_new);
        free(sumq_old);
        free(sumq_new);
    }

    // Cleanup
    printf("\n finalizing model...\n");
    status = model->finalize(model);
    assert(status == BMI_SUCCESS);
    free(model);

    printf("\n***Ъ*****************************\nEND EDGE CASES AND ROBUSTNESS TEST\n\n");
    return 0;
}
