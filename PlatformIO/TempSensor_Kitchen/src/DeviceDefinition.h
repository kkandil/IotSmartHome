#pragma once
#include "SmartSnapAPI.h"
// Imported once when a NEW board is added; existing server variables are never overwritten.
static const SmartSnap::VariableDefinition DEVICE_VARIABLES[] = {
    {"Temp", "float", "0", false},
    {"Humid", "float", "0", false},
    {"status", "string", "0", false},
    {"update", "int", "0", false},
};
