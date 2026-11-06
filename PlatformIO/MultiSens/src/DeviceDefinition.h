#pragma once
#include "SmartSnapAPI.h"
// Imported once when a NEW board is added; existing server variables are never overwritten.
static const SmartSnap::VariableDefinition DEVICE_VARIABLES[] = {
    {"Light", "float", "0", false},
    {"Temp", "float", "0", false},
    {"Humd", "float", "0", false},
    {"Pressure", "float", "0", false},
    {"Update", "int", "0", false},
    {"Presence", "int", "0", false},
    {"Distance", "int", "0", false},
    {"ReportInterval", "int", "15", true},
    {"LightThreshold", "float", "13", true},
    {"LightUpdateThr", "float", "50", true},
    {"TempUpdateThreshold", "float", "0.5", true},
    {"status", "string", "0", false},
};
