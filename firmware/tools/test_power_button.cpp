#include "services/PowerButtonGate.h"
#include <cassert>
#include <cstdint>
#include <initializer_list>

int main() {
    using Gate = PowerButtonGate;
    Gate unpressed;
    assert(unpressed.update(false, 0) == Gate::SLEEP); // reset/USB is not power-on
    for (uint32_t held : {0u, 20u, 300u, 1199u}) {
        Gate short_press;
        assert(short_press.update(true, 100) == Gate::WAIT);
        assert(short_press.update(false, 100 + held) == Gate::SLEEP);
    }
    Gate long_press;
    assert(long_press.update(true, 100) == Gate::WAIT);
    assert(long_press.update(true, 1299) == Gate::WAIT);
    assert(long_press.update(true, 1300) == Gate::WAIT); // qualified, still held
    assert(long_press.update(true, 5000) == Gate::WAIT); // no input leaks to UI
    assert(long_press.update(false, 5005) == Gate::WAIT);
    assert(long_press.update(false, 5034) == Gate::WAIT);
    assert(long_press.update(true, 5035) == Gate::WAIT); // contact bounce
    assert(long_press.update(false, 5040) == Gate::WAIT);
    assert(long_press.update(false, 5070) == Gate::START);

    Gate rollover;
    constexpr uint32_t start = UINT32_MAX - 600;
    assert(rollover.update(true, start) == Gate::WAIT);
    assert(rollover.update(true, start + 1200u) == Gate::WAIT);
    assert(rollover.update(false, start + 1210u) == Gate::WAIT);
    assert(rollover.update(false, start + 1240u) == Gate::START);
}
