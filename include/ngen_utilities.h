#ifndef TOPMODEL_NGEN_UTILITIES_H
#define TOPMODEL_NGEN_UTILITIES_H

/*
 * ngen serialization protocol -- opt-in BMI state checkpoint/restore.
 *
 * The four reserved variables are discovered by name rather than enumeration, so
 * they must stay out of GetInputVarNames/GetOutputVarNames while still resolving
 * through GetVarType, GetVarUnits, GetVarItemsize and GetVarNbytes.  The engine's
 * support probe is GetVarUnits alone, compared exactly against the units below.
 *
 * Save:    SetValue(create) -> GetValue(size) -> GetValue(state) -> SetValue(free)
 * Restore: SetValue(state, payload)
 */

#include <string.h>

#define NGEN_SERIALIZATION_CREATE "ngen::serialization_create"
#define NGEN_SERIALIZATION_FREE   "ngen::serialization_free"
#define NGEN_SERIALIZATION_SIZE   "ngen::serialization_size"
#define NGEN_SERIALIZATION_STATE  "ngen::serialization_state"

/* Index into each of the tables below */
enum {
    SER_VAR_CREATE = 0,
    SER_VAR_FREE,
    SER_VAR_SIZE,
    SER_VAR_STATE,
    SER_VAR_COUNT
};

static const char *serialization_var_names[SER_VAR_COUNT] = {
    NGEN_SERIALIZATION_CREATE, NGEN_SERIALIZATION_FREE,
    NGEN_SERIALIZATION_SIZE,   NGEN_SERIALIZATION_STATE
};

static const char *serialization_var_types[SER_VAR_COUNT] = {
    "int", "int", "int", "char"
};

static const char *serialization_var_units[SER_VAR_COUNT] = {
    "ngen::trigger", "ngen::trigger", "bytes", "ngen::opaque"
};

/* state is sized by the model, so it gets a count of 0 here */
static const int serialization_var_item_count[SER_VAR_COUNT] = {1, 1, 1, 0};

/* Index of a reserved variable, or -1 if name is not one */
static inline int serialization_var_index(const char *name)
{
    for (int i = 0; i < SER_VAR_COUNT; i++) {
        if (strcmp(name, serialization_var_names[i]) == 0)
            return i;
    }
    return -1;
}

#endif
