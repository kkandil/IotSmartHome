#pragma once
#include <SmartSnapAPI.h>
static const SmartSnap::VariableDefinition DEVICE_VARIABLES[] = {
    {"Volume", "int", "15", true},
    {"DataSourceUrl", "string", "https://mawaqit.net/de/m/izstuttgart", true},
    {"Fajr", "int", "1", true},
    {"Dhuhr", "int", "1", true},
    {"Asr", "int", "1", true},
    {"Maghrib", "int", "1", true},
    {"Isha", "int", "1", true},
    {"Stop", "int", "0", false},
};
