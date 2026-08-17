/*
 * Conformance and round-trip tests for the ngen BMI serialization protocol.
 *
 * The static checks reproduce the host engine's support probe, which is a
 * GetVarUnits call on each reserved name compared exactly against the protocol's
 * reserved unit strings.
 *
 * NOTE: run from one level below the repo root (the build dir) -- the .run config
 * names its inputs relative to cwd, as simple_unit_tests also requires.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/bmi.h"
#include "../include/bmi_topmodel.h"
#include "../include/topmodel.h"

#define CHECK(cond, ...)          \
    do {                          \
        if (!(cond)) {            \
            printf("   FAIL: ");  \
            printf(__VA_ARGS__);  \
            printf("\n");         \
            failures++;           \
        }                         \
    } while (0)

static int failures = 0;

/* Spelled out rather than read from ngen_utilities.h, so that a wrong table is
   caught instead of agreeing with itself. */
static const char *reserved_names[4] = {
    "ngen::serialization_create", "ngen::serialization_free",
    "ngen::serialization_size",   "ngen::serialization_state"
};
static const char *reserved_units[4] = {
    "ngen::trigger", "ngen::trigger", "bytes", "ngen::opaque"
};
static const char *reserved_types[4] = {"int", "int", "int64", "char"};
static const int reserved_itemsizes[4] = {
    (int)sizeof(int), (int)sizeof(int), (int)sizeof(int64_t), (int)sizeof(char)
};

static Bmi *make_model(void) {
    Bmi *model = (Bmi *)malloc(sizeof(Bmi));
    assert(model != NULL);
    register_bmi_topmodel(model);

    const char *cfg_file = REPO_ROOT_DIR "/test/data/topmod_unit_test.run";
    if (model->initialize(model, cfg_file) == BMI_FAILURE) {
        printf("   FAIL: could not initialize from %s\n", cfg_file);
        printf("         (run from one level below the repo root)\n");
        exit(BMI_FAILURE);
    }
    return model;
}

static void destroy_model(Bmi *model) {
    model->finalize(model);
    free(model);
}

/* Deterministic forcings, so two runs over the same indices agree exactly */
static void step_range(Bmi *model, int from, int to) {
    for (int i = from; i < to; i++) {
        double precip = 0.001 * (i + 1);
        double pet = 0.0005;
        model->set_value(model, "atmosphere_water__liquid_equivalent_precipitation_rate", &precip);
        model->set_value(model, "water_potential_evaporation_flux", &pet);
        model->update(model);
    }
}

static void step(Bmi *model, int steps) { step_range(model, 0, steps); }

/* Capture through the sequence the engine drives.  Caller frees *out. */
static int capture(Bmi *model, char **out, int64_t *out_size) {
    int trigger = 0;
    if (model->set_value(model, "ngen::serialization_create", &trigger) != BMI_SUCCESS)
        return BMI_FAILURE;

    int64_t size = -1;
    if (model->get_value(model, "ngen::serialization_size", &size) != BMI_SUCCESS)
        return BMI_FAILURE;
    if (size <= 0)
        return BMI_FAILURE;

    char *buffer = (char *)malloc((size_t)size);
    assert(buffer != NULL);
    if (model->get_value(model, "ngen::serialization_state", buffer) != BMI_SUCCESS) {
        free(buffer);
        return BMI_FAILURE;
    }
    if (model->set_value(model, "ngen::serialization_free", &trigger) != BMI_SUCCESS) {
        free(buffer);
        return BMI_FAILURE;
    }

    *out = buffer;
    *out_size = size;
    return BMI_SUCCESS;
}

/* Restore through the sequence the engine drives: the byte count, then the bytes */
static int restore(Bmi *model, char *payload, int64_t size) {
    if (model->set_value(model, "ngen::serialization_size", &size) != BMI_SUCCESS)
        return BMI_FAILURE;
    return model->set_value(model, "ngen::serialization_state", payload);
}

static void test_units_probe(Bmi *model) {
    printf("\n[1] GetVarUnits support probe\n");
    for (int i = 0; i < 4; i++) {
        char units[BMI_MAX_UNITS_NAME];
        memset(units, 0, sizeof(units));
        CHECK(model->get_var_units(model, reserved_names[i], units) == BMI_SUCCESS,
              "get_var_units(%s) failed; the engine's probe disables the protocol here",
              reserved_names[i]);
        CHECK(strcmp(units, reserved_units[i]) == 0,
              "get_var_units(%s) = \"%s\", expected \"%s\"",
              reserved_names[i], units, reserved_units[i]);
        printf("   %s -> \"%s\"\n", reserved_names[i], units);
    }
}

static void test_introspection(Bmi *model) {
    printf("\n[2] GetVarType, GetVarItemsize and GetVarNbytes resolve\n");
    for (int i = 0; i < 4; i++) {
        char type[BMI_MAX_TYPE_NAME];
        int itemsize = -1, nbytes = -1;
        memset(type, 0, sizeof(type));

        CHECK(model->get_var_type(model, reserved_names[i], type) == BMI_SUCCESS,
              "get_var_type(%s) failed", reserved_names[i]);
        CHECK(strcmp(type, reserved_types[i]) == 0,
              "get_var_type(%s) = \"%s\", expected \"%s\"",
              reserved_names[i], type, reserved_types[i]);
        CHECK(model->get_var_itemsize(model, reserved_names[i], &itemsize) == BMI_SUCCESS,
              "get_var_itemsize(%s) failed", reserved_names[i]);
        CHECK(itemsize == reserved_itemsizes[i],
              "get_var_itemsize(%s) = %d, expected %d",
              reserved_names[i], itemsize, reserved_itemsizes[i]);
        CHECK(model->get_var_nbytes(model, reserved_names[i], &nbytes) == BMI_SUCCESS,
              "get_var_nbytes(%s) failed", reserved_names[i]);
        printf("   %s -> type=%s itemsize=%d nbytes=%d\n",
               reserved_names[i], type, itemsize, nbytes);
    }
}

static void test_nbytes_before_create(Bmi *model) {
    printf("\n[3] GetVarNbytes(state) resolves before any create\n");
    int nbytes = -1;
    CHECK(model->get_var_nbytes(model, "ngen::serialization_state", &nbytes) == BMI_SUCCESS,
          "get_var_nbytes(state) failed with no state held; a restore precedes the first create");
    CHECK(nbytes == 0, "expected 0 bytes with no state held, got %d", nbytes);
    printf("   nbytes with no state held = %d\n", nbytes);
}

static void test_names_not_enumerated(Bmi *model) {
    printf("\n[4] Reserved names absent from the item lists\n");
    int count_in = 0, count_out = 0;
    model->get_input_item_count(model, &count_in);
    model->get_output_item_count(model, &count_out);

    char **names_in = (char **)malloc(sizeof(char *) * count_in);
    char **names_out = (char **)malloc(sizeof(char *) * count_out);
    assert(names_in != NULL && names_out != NULL);
    for (int i = 0; i < count_in; i++)
        names_in[i] = (char *)malloc(BMI_MAX_VAR_NAME);
    for (int i = 0; i < count_out; i++)
        names_out[i] = (char *)malloc(BMI_MAX_VAR_NAME);

    model->get_input_var_names(model, names_in);
    model->get_output_var_names(model, names_out);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < count_in; j++)
            CHECK(strcmp(names_in[j], reserved_names[i]) != 0,
                  "%s is in GetInputVarNames", reserved_names[i]);
        for (int j = 0; j < count_out; j++)
            CHECK(strcmp(names_out[j], reserved_names[i]) != 0,
                  "%s is in GetOutputVarNames", reserved_names[i]);
    }
    printf("   checked %d input and %d output names\n", count_in, count_out);

    for (int i = 0; i < count_in; i++) free(names_in[i]);
    for (int i = 0; i < count_out; i++) free(names_out[i]);
    free(names_in);
    free(names_out);
}

static void test_spatial_calls_decline(Bmi *model) {
    printf("\n[5] GetVarGrid and GetVarLocation decline\n");
    for (int i = 0; i < 4; i++) {
        int grid;
        char location[BMI_MAX_LOCATION_NAME];
        CHECK(model->get_var_grid(model, reserved_names[i], &grid) == BMI_FAILURE,
              "get_var_grid(%s) succeeded", reserved_names[i]);
        CHECK(model->get_var_location(model, reserved_names[i], location) == BMI_FAILURE,
              "get_var_location(%s) succeeded", reserved_names[i]);
    }
    printf("   all four decline both calls\n");
}

/* TODO: hotstart is the only restore driven at present; a full resume,
   continuing the same simulation, will need its own coverage here.
   A restore applies every archived value except the clock, which stays with the
   run doing the restoring. */
static void test_round_trip(void) {
    printf("\n[6] Save, diverge, restore\n");
    Bmi *model = make_model();
    step(model, 4);

    topmodel_model *m = (topmodel_model *)model->data;
    int saved_step = m->current_time_step;
    double saved_sbar = m->sbar, saved_sumq = m->sumq, saved_sump = m->sump;
    double saved_deficit = m->deficit_local[0];

    char *payload = NULL;
    int64_t payload_size = 0;
    CHECK(capture(model, &payload, &payload_size) == BMI_SUCCESS, "capture failed");
    if (payload == NULL) { destroy_model(model); return; }
    printf("   captured %lld bytes at step %d\n", (long long)payload_size, saved_step);

    CHECK(m->current_time_step == saved_step && m->sbar == saved_sbar && m->sumq == saved_sumq,
          "create/free altered computed state");

    step(model, 3);
    CHECK(m->current_time_step != saved_step, "model did not advance after capture");
    int diverged_step = m->current_time_step;

    CHECK(restore(model, payload, payload_size) == BMI_SUCCESS, "restore failed");

    CHECK(m->sbar == saved_sbar, "sbar = %g, expected %g", m->sbar, saved_sbar);
    CHECK(m->sumq == saved_sumq, "sumq = %g, expected %g", m->sumq, saved_sumq);
    CHECK(m->sump == saved_sump, "sump = %g, expected %g", m->sump, saved_sump);
    CHECK(m->deficit_local[0] == saved_deficit,
          "deficit_local[0] = %g, expected %g", m->deficit_local[0], saved_deficit);

    CHECK(m->current_time_step == diverged_step,
          "restore moved the clock to %d; a hotstart keeps its own", m->current_time_step);
    printf("   state restored, clock left at step %d\n", m->current_time_step);

    free(payload);
    destroy_model(model);
}

static void test_restore_matches_uninterrupted(void) {
    printf("\n[7] Restored run matches an uninterrupted one\n");
    Bmi *reference = make_model();
    step(reference, 7);
    topmodel_model *r = (topmodel_model *)reference->data;
    double ref_sbar = r->sbar, ref_sumq = r->sumq, ref_Qout = r->Qout;

    Bmi *interrupted = make_model();
    step(interrupted, 4);
    char *payload = NULL;
    int64_t payload_size = 0;
    if (capture(interrupted, &payload, &payload_size) != BMI_SUCCESS) {
        CHECK(0, "capture failed");
        destroy_model(reference);
        destroy_model(interrupted);
        return;
    }
    step(interrupted, 3);
    CHECK(restore(interrupted, payload, payload_size) == BMI_SUCCESS, "restore failed");
    step_range(interrupted, 4, 7);

    topmodel_model *n = (topmodel_model *)interrupted->data;
    CHECK(n->sbar == ref_sbar, "sbar = %g, uninterrupted gave %g", n->sbar, ref_sbar);
    CHECK(n->sumq == ref_sumq, "sumq = %g, uninterrupted gave %g", n->sumq, ref_sumq);
    CHECK(n->Qout == ref_Qout, "Qout = %g, uninterrupted gave %g", n->Qout, ref_Qout);
    printf("   sbar=%g sumq=%g Qout=%g in both runs\n", n->sbar, n->sumq, n->Qout);

    free(payload);
    destroy_model(reference);
    destroy_model(interrupted);
}

static void test_free_is_safe(void) {
    printf("\n[8] free is safe whenever it is issued\n");
    Bmi *model = make_model();
    step(model, 2);
    int trigger = 0;

    CHECK(model->set_value(model, "ngen::serialization_free", &trigger) == BMI_SUCCESS,
          "free before any create failed");
    CHECK(model->set_value(model, "ngen::serialization_create", &trigger) == BMI_SUCCESS,
          "create failed");
    CHECK(model->set_value(model, "ngen::serialization_free", &trigger) == BMI_SUCCESS,
          "first free failed");
    CHECK(model->set_value(model, "ngen::serialization_free", &trigger) == BMI_SUCCESS,
          "second free failed");

    int64_t size = -1;
    CHECK(model->get_value(model, "ngen::serialization_size", &size) == BMI_SUCCESS,
          "size unreadable after free");
    CHECK(size == 0, "size = %lld after free, expected 0", (long long)size);
    printf("   free before create and twice in a row both leave size = %lld\n", (long long)size);

    destroy_model(model);
}

/* Offset of Boost's class version field, located from the archive signature so a
   layout change fails loudly rather than silently testing nothing. */
static int layout_version_offset(const char *payload, int64_t size) {
    static const char sig[] = "serialization::archive";
    for (int i = 0; i + (int)sizeof(sig) < size; i++) {
        if (memcmp(payload + i, sig, sizeof(sig) - 1) == 0)
            return i + (int)(sizeof(sig) - 1) + 11;
    }
    return -1;
}

static void test_rejects_bad_payloads(void) {
    printf("\n[9] Corrupt and stale payloads are rejected\n");
    Bmi *model = make_model();
    step(model, 4);

    char *payload = NULL;
    int64_t payload_size = 0;
    if (capture(model, &payload, &payload_size) != BMI_SUCCESS) {
        CHECK(0, "capture failed");
        destroy_model(model);
        return;
    }

    topmodel_model *m = (topmodel_model *)model->data;
    double sbar_before = m->sbar;
    char *corrupt = (char *)malloc((size_t)payload_size);
    assert(corrupt != NULL);

    int sig_at = layout_version_offset(payload, payload_size);
    CHECK(sig_at > 0, "could not locate the archive signature in the payload");

    memcpy(corrupt, payload, (size_t)payload_size);
    corrupt[sig_at - 20] = (char)(corrupt[sig_at - 20] + 1);
    CHECK(restore(model, corrupt, payload_size) == BMI_FAILURE,
          "a payload with a corrupted signature was accepted");
    CHECK(m->sbar == sbar_before, "a rejected payload altered model state");

    if (sig_at > 0) {
        CHECK(payload[sig_at] == 1,
              "expected layout version 1 at offset %d, found %d; Boost's archive layout moved",
              sig_at, (int)payload[sig_at]);
        memcpy(corrupt, payload, (size_t)payload_size);
        corrupt[sig_at] = (char)(corrupt[sig_at] + 1);
        CHECK(restore(model, corrupt, payload_size) == BMI_FAILURE,
              "a payload with an unknown layout version was accepted");
        CHECK(m->sbar == sbar_before, "a rejected payload altered model state");
    }

    printf("   corrupted signature and layout version both rejected\n");

    free(corrupt);
    free(payload);
    destroy_model(model);
}

/* The clock is the only field the mode discriminates, and the mode is fixed at
   hotstart today, so this covers the other branch and keeps it from quietly
   becoming dead code. */
static void test_resume_mode_applies_the_clock(void) {
    printf("\n[10] Resume mode takes the clock from the snapshot\n");
    Bmi *model = make_model();
    step(model, 4);

    topmodel_model *m = (topmodel_model *)model->data;
    int saved_step = m->current_time_step;

    char *payload = NULL;
    int64_t payload_size = 0;
    if (capture(model, &payload, &payload_size) != BMI_SUCCESS) {
        CHECK(0, "capture failed");
        destroy_model(model);
        return;
    }
    step(model, 3);

    m->restore_mode = TOPMODEL_RESTORE_RESUME;
    CHECK(restore(model, payload, payload_size) == BMI_SUCCESS, "restore failed");
    CHECK(m->current_time_step == saved_step,
          "current_time_step = %d, expected the snapshot's %d", m->current_time_step, saved_step);
    printf("   clock taken from the snapshot at step %d\n", m->current_time_step);

    free(payload);
    destroy_model(model);
}

static void test_size_is_settable(void) {
    printf("\n[11] Size can be set, and reads back\n");
    Bmi *model = make_model();

    int64_t declared = 4096;
    CHECK(model->set_value(model, "ngen::serialization_size", &declared) == BMI_SUCCESS,
          "size is not settable; a restore cannot declare its byte count");

    int64_t reported = -1;
    CHECK(model->get_value(model, "ngen::serialization_size", &reported) == BMI_SUCCESS,
          "size unreadable after being set");
    CHECK(reported == declared, "size = %lld, expected %lld",
          (long long)reported, (long long)declared);
    printf("   set %lld, read back %lld\n", (long long)declared, (long long)reported);

    destroy_model(model);
}

static void test_nbytes_reflects_set_size(void) {
    printf("\n[12] GetVarNbytes(state) reports a declared size\n");
    Bmi *model = make_model();

    int64_t declared = 4096;
    CHECK(model->set_value(model, "ngen::serialization_size", &declared) == BMI_SUCCESS,
          "size is not settable");

    int nbytes = -1;
    CHECK(model->get_var_nbytes(model, "ngen::serialization_state", &nbytes) == BMI_SUCCESS,
          "get_var_nbytes(state) failed");
    CHECK(nbytes == (int)declared, "nbytes = %d, expected %lld",
          nbytes, (long long)declared);
    printf("   nbytes = %d with %lld declared\n", nbytes, (long long)declared);

    destroy_model(model);
}

/* The payload carries no length of its own, so undeclared bytes are unreadable. */
static void test_state_without_size_fails(void) {
    printf("\n[13] State with no declared size is rejected\n");
    Bmi *model = make_model();
    step(model, 4);

    char *payload = NULL;
    int64_t payload_size = 0;
    if (capture(model, &payload, &payload_size) != BMI_SUCCESS) {
        CHECK(0, "capture failed");
        destroy_model(model);
        return;
    }

    /* capture ends on free, which clears the size along with the buffer */
    topmodel_model *m = (topmodel_model *)model->data;
    double sbar_before = m->sbar;
    CHECK(model->set_value(model, "ngen::serialization_state", payload) == BMI_FAILURE,
          "state was accepted with no size declared");
    CHECK(m->sbar == sbar_before, "a rejected payload altered model state");
    printf("   %lld bytes turned away with no size declared\n", (long long)payload_size);

    free(payload);
    destroy_model(model);
}

int main(void) {
#ifndef REPO_ROOT_DIR
    printf("Please set REPO_ROOT_DIR build macro and rebuild test executable\n");
    return BMI_FAILURE;
#endif

    printf("\nBEGIN BMI SERIALIZATION CONFORMANCE TEST\n");
    printf("***************************************\n");

    Bmi *model = make_model();
    test_units_probe(model);
    test_introspection(model);
    test_nbytes_before_create(model);
    test_names_not_enumerated(model);
    test_spatial_calls_decline(model);
    destroy_model(model);

    test_round_trip();
    test_restore_matches_uninterrupted();
    test_free_is_safe();
    test_rejects_bad_payloads();
    test_resume_mode_applies_the_clock();
    test_size_is_settable();
    test_nbytes_reflects_set_size();
    test_state_without_size_fails();

    printf("\n***************************************\n");
    if (failures == 0) {
        printf("END BMI SERIALIZATION CONFORMANCE TEST -- all checks passed\n\n");
        return BMI_SUCCESS;
    }
    printf("END BMI SERIALIZATION CONFORMANCE TEST -- %d check(s) FAILED\n\n", failures);
    return BMI_FAILURE;
}
