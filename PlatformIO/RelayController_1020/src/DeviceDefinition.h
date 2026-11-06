#pragma once
#include "SmartSnapAPI.h"
// Imported once when a NEW board is added; existing server variables are never overwritten.
static const SmartSnap::VariableDefinition DEVICE_VARIABLES[] = {
    {"Relay1", "int", "0", false},
    {"Relay2", "int", "0", false},
    {"Relay3", "int", "0", false},
};
