/*
 * Long-run restart equivalence for the ngen BMI serialization protocol.
 *
 * Runs the same forcing sequence three ways -- straight through, with one
 * checkpoint/restart, and with three.  Instantaneous outputs must match exactly at
 * every step; the run integrals restart at each boundary and are checked against
 * the reference's increment since then.  Each restart goes through a file and a
 * brand new BMI instance, with the previous one finalized and freed first, so
 * state left dangling in the old instance shows up rather than being read by luck.
 *
 * Note what the current fixture cannot reach.  It resolves to num_delay 0 and
 * num_time_delay_histo_ords 1, so Q holds two elements either way and
 * time_delay_histogram is config derived and never written during update.
 * Mis-sizing Q or dropping the histogram therefore both go undetected here; a
 * catchment with real channel delay would be needed to cover them.  Dropping
 * genuinely evolving state, sbar for one, is caught at the first step after a
 * restart.
 *
 * NOTE: run from one level below the repo root (the build dir) -- the .run config
 * names its inputs relative to cwd, as the other test targets also require.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/bmi.h"
#include "../include/bmi_topmodel.h"

#define CHECKPOINT_FILE "topmodel_integration_checkpoint.bin"

static const char *forcing_file = REPO_ROOT_DIR "/test/data/inputs.dat";
static const char *config_file = REPO_ROOT_DIR "/test/data/topmod_unit_test.run";

static int n_steps = 0;
static double *rain = NULL;
static double *pe = NULL;

static int n_out = 0;
static char **out_names = NULL;

static int failures = 0;

static void load_forcing(void) {
    FILE *f = fopen(forcing_file, "r");
    if (f == NULL) {
        printf("   FAIL: could not open %s\n", forcing_file);
        printf("         (run from one level below the repo root)\n");
        exit(BMI_FAILURE);
    }
    double dt = 0.0;
    if (fscanf(f, "%d %lf", &n_steps, &dt) != 2) {
        printf("   FAIL: could not read the header of %s\n", forcing_file);
        exit(BMI_FAILURE);
    }
    rain = (double *)malloc(sizeof(double) * n_steps);
    pe = (double *)malloc(sizeof(double) * n_steps);
    assert(rain != NULL && pe != NULL);
    for (int i = 0; i < n_steps; i++) {
        double qobs;
        if (fscanf(f, "%lf %lf %lf", &rain[i], &pe[i], &qobs) != 3) {
            printf("   FAIL: %s ended after %d of %d records\n", forcing_file, i, n_steps);
            exit(BMI_FAILURE);
        }
    }
    fclose(f);
}

static Bmi *make_model(void) {
    Bmi *model = (Bmi *)malloc(sizeof(Bmi));
    assert(model != NULL);
    register_bmi_topmodel(model);
    if (model->initialize(model, config_file) == BMI_FAILURE) {
        printf("   FAIL: could not initialize from %s\n", config_file);
        exit(BMI_FAILURE);
    }
    return model;
}

static void destroy_model(Bmi *model) {
    model->finalize(model);
    free(model);
}

static void load_output_names(void) {
    Bmi *model = make_model();
    model->get_output_item_count(model, &n_out);
    out_names = (char **)malloc(sizeof(char *) * n_out);
    assert(out_names != NULL);
    for (int i = 0; i < n_out; i++) {
        out_names[i] = (char *)malloc(BMI_MAX_VAR_NAME);
        assert(out_names[i] != NULL);
    }
    model->get_output_var_names(model, out_names);
    destroy_model(model);
}

/* Capture through the engine's sequence and write the payload to a file. */
static void checkpoint(Bmi *model) {
    int trigger = 0, size = -1;
    if (model->set_value(model, "ngen::serialization_create", &trigger) != BMI_SUCCESS) {
        printf("   FAIL: create failed\n");
        exit(BMI_FAILURE);
    }
    if (model->get_value(model, "ngen::serialization_size", &size) != BMI_SUCCESS || size <= 0) {
        printf("   FAIL: size read back as %d\n", size);
        exit(BMI_FAILURE);
    }
    char *payload = (char *)malloc((size_t)size);
    assert(payload != NULL);
    if (model->get_value(model, "ngen::serialization_state", payload) != BMI_SUCCESS) {
        printf("   FAIL: state read failed\n");
        exit(BMI_FAILURE);
    }
    model->set_value(model, "ngen::serialization_free", &trigger);

    FILE *f = fopen(CHECKPOINT_FILE, "wb");
    assert(f != NULL);
    fwrite(payload, 1, (size_t)size, f);
    fclose(f);
    free(payload);
}

static void restore(Bmi *model) {
    FILE *f = fopen(CHECKPOINT_FILE, "rb");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *payload = (char *)malloc((size_t)size);
    assert(payload != NULL);
    size_t got = fread(payload, 1, (size_t)size, f);
    fclose(f);
    assert(got == (size_t)size);

    if (model->set_value(model, "ngen::serialization_state", payload) != BMI_SUCCESS) {
        printf("   FAIL: restore failed\n");
        exit(BMI_FAILURE);
    }
    free(payload);
}

/* Step through the whole forcing record, restarting after each listed step.
   Outputs land in results as [step * n_out + variable]. */
static void run(const int *restart_after, int n_restarts, double *results) {
    Bmi *model = make_model();
    int next = 0;
    for (int step = 0; step < n_steps; step++) {
        model->set_value(model, "atmosphere_water__liquid_equivalent_precipitation_rate", &rain[step]);
        model->set_value(model, "water_potential_evaporation_flux", &pe[step]);
        model->update(model);

        for (int v = 0; v < n_out; v++)
            model->get_value(model, out_names[v], &results[step * n_out + v]);

        if (next < n_restarts && step == restart_after[next]) {
            checkpoint(model);
            destroy_model(model);
            model = make_model();
            restore(model);
            next++;
        }
    }
    destroy_model(model);
}

/* TODO: hotstart is the only restore supported at present; a full resume,
   continuing the same simulation, would carry these across a restart too. */
static int is_run_integral(const char *name) {
    return strstr(name, "domain_time_integral_of_precipitation") != NULL
        || strstr(name, "domain_time_integral_of_evaporation") != NULL
        || strstr(name, "domain_time_integral_of_runoff") != NULL;
}

/* Instantaneous outputs must match exactly.  The run integrals restart at each
   boundary, so they are checked against the reference's increment since that
   boundary; summing a tail is not bit-identical to differencing two totals, hence
   the tolerance. */
static void compare(const char *label, const double *reference, const double *actual,
                    const int *restart_after, int n_restarts) {
    int baseline = -1;
    int next = 0;
    for (int step = 0; step < n_steps; step++) {
        for (int v = 0; v < n_out; v++) {
            int i = step * n_out + v;
            if (is_run_integral(out_names[v])) {
                double base = baseline < 0 ? 0.0 : reference[baseline * n_out + v];
                double expected = reference[i] - base;
                double scale = fabs(expected) > 1.0 ? fabs(expected) : 1.0;
                if (fabs(actual[i] - expected) > 1e-9 * scale) {
                    printf("   FAIL: %s diverges at step %d, %s: %.17g vs %.17g since the restart\n",
                           label, step, out_names[v], actual[i], expected);
                    failures++;
                    return;
                }
            } else if (reference[i] != actual[i]) {
                printf("   FAIL: %s diverges at step %d, %s: %.17g vs %.17g\n",
                       label, step, out_names[v], actual[i], reference[i]);
                failures++;
                return;
            }
        }
        if (next < n_restarts && step == restart_after[next]) {
            baseline = step;
            next++;
        }
    }
    printf("   %s matches the straight run over %d steps and %d variables\n",
           label, n_steps, n_out);
}

int main(void) {
#ifndef REPO_ROOT_DIR
    printf("Please set REPO_ROOT_DIR build macro and rebuild test executable\n");
    return BMI_FAILURE;
#endif

    printf("\nBEGIN BMI SERIALIZATION INTEGRATION TEST\n");
    printf("***************************************\n");

    load_forcing();
    load_output_names();
    printf("\n%d steps of forcing from %s, %d output variables\n",
           n_steps, forcing_file, n_out);

    size_t n = (size_t)n_steps * n_out;
    double *straight = (double *)malloc(sizeof(double) * n);
    double *one_restart = (double *)malloc(sizeof(double) * n);
    double *three_restarts = (double *)malloc(sizeof(double) * n);
    assert(straight != NULL && one_restart != NULL && three_restarts != NULL);

    const int one[1] = {n_steps / 2};
    const int three[3] = {n_steps / 4, n_steps / 2, (3 * n_steps) / 4};

    printf("\n[1] Straight through\n");
    run(NULL, 0, straight);
    printf("   completed %d steps\n", n_steps);

    printf("\n[2] One restart, after step %d\n", one[0]);
    run(one, 1, one_restart);
    compare("one restart", straight, one_restart, one, 1);

    printf("\n[3] Three restarts, after steps %d, %d and %d\n", three[0], three[1], three[2]);
    run(three, 3, three_restarts);
    compare("three restarts", straight, three_restarts, three, 3);

    remove(CHECKPOINT_FILE);
    free(straight);
    free(one_restart);
    free(three_restarts);
    for (int i = 0; i < n_out; i++) free(out_names[i]);
    free(out_names);
    free(rain);
    free(pe);

    printf("\n***************************************\n");
    if (failures == 0) {
        printf("END BMI SERIALIZATION INTEGRATION TEST -- all checks passed\n\n");
        return BMI_SUCCESS;
    }
    printf("END BMI SERIALIZATION INTEGRATION TEST -- %d check(s) FAILED\n\n", failures);
    return BMI_FAILURE;
}
