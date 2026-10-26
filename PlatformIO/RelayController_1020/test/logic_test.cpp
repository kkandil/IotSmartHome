#include <assert.h>
#include <stdio.h>
#include "../src/RelayLogic.h"
#include "../src/StartupState.h"
#include "../src/ServerRestore.h"

int main() {
    ServerRestore restore;
    restore.connect(true, 100);
    assert(restore.waiting[0] && restore.waiting[1] && restore.waiting[2]);
    assert(!restore.retryDue(3099));
    assert(restore.retryDue(3100));
    restore.accept(0); // A server response, newer live command or wall switch wins.
    assert(!restore.waiting[0] && restore.waiting[1]);
    restore.accept(1);restore.accept(2);
    assert(!restore.retryDue(6100));
    restore.connect(true, 7000);assert(restore.waiting[0]); // Every reconnect.
    restore.connect(false, 8000);assert(!restore.waiting[0] && !restore.waiting[2]);
    StartupState startup;
    assert(!startup.canKeepContacts(true)); // Default: OFF even with valid storage.
    startup.preserve = true;
    startup.states = 5;
    assert(startup.canKeepContacts(true)); // Clean journal retains ON and OFF.
    assert(!startup.canKeepContacts(false)); // Missing/corrupt storage: reset OFF.
    startup.pending = true;
    assert(!startup.canKeepContacts(true)); // Interrupted coil pulse: reset OFF.
    for (uint8_t i = 0; i < 3; ++i) {
        assert(coilMask(i, false) == (1 << (2 * i)));
        assert(coilMask(i, true) == (1 << (2 * i + 1)));
        assert((coilMask(i, true) & coilMask(i, false)) == 0);
    }
    assert(coilMask(3, true) == 0);
    SwitchInput sw;
    sw.begin(false, 0); // Boot with switch closed must not toggle.
    assert(!sw.update(false, 100, 40, false));
    assert(!sw.update(true, 110, 40, false));
    assert(!sw.update(false, 120, 40, false)); // Contact bounce.
    assert(!sw.update(true, 130, 40, false));
    assert(!sw.update(true, 169, 40, false));
    assert(sw.update(true, 170, 40, false));
    assert(!sw.update(true, 200, 40, false)); // Held: no repeat.
    assert(!sw.update(false, 210, 40, false));
    assert(sw.update(false, 250, 40, false)); // Both edges toggle.
    sw.begin(true, 0xfffffff0);
    assert(!sw.update(false, 0xfffffff5, 40, false));
    assert(sw.update(false, 29, 40, false)); // millis rollover.
    sw.begin(false, 0);
    assert(!sw.update(true, 1, 40, true));
    assert(!sw.update(true, 41, 40, true)); // Momentary release ignored.
    assert(!sw.update(false, 42, 40, true));
    assert(sw.update(false, 82, 40, true));
    puts("PASS: coil mapping, startup, bounce, both edges, hold, rollover, momentary");
}
