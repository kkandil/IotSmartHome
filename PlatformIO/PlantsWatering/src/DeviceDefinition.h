#pragma once
#include "SmartSnapAPI.h"
// Imported once when a NEW board is added; existing server variables are never overwritten.
static const SmartSnap::VariableDefinition DEVICE_VARIABLES[] = {
    {"V7", "int", "0", false},
    {"V8", "int", "0", false},
    {"V6", "int", "0", false},
    {"V1", "int", "0", false},
    {"V2", "int", "0", false},
    {"V3", "string", "0", false},
    {"V4", "string", "0", false},
    {"V5", "int", "0", false},
};
