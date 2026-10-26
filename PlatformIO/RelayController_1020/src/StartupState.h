#pragma once
#include <stdint.h>

struct StartupState {
    bool preserve = false;
    uint8_t states = 0;
    bool pending = false;
    bool canKeepContacts(bool valid) const { return valid && preserve && !pending; }
};
