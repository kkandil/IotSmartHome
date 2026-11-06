#pragma once
#include "SmartSnapAPI.h"
// Imported once when a NEW board is added; existing server variables are never overwritten.
static const SmartSnap::VariableDefinition DEVICE_VARIABLES[] = {
    {"V1", "int", "0", false},
    {"V2", "float", "0", false},
};
