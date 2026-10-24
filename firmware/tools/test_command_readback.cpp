#include "services/LightControl.h"
#include "services/CodexEventGate.h"
#include <cassert>
#include <cstdio>

int main() {
    // State-before-ACK confirms immediately. ACK-before-state, an old command's
    // readback, an offline state or a stale snapshot must never confirm.
    assert(light_control_readback_for_command(true,true,true,"new","new",110,100));
    assert(!light_control_readback_for_command(true,true,true,"old","new",110,100));
    assert(!light_control_readback_for_command(true,false,true,"new","new",110,100));
    assert(!light_control_readback_for_command(true,true,false,"new","new",110,100));
    assert(!light_control_readback_for_command(true,true,true,"new","new",90,100));
    assert(!light_control_readback_for_command(true,true,true,"","",110,100));
    assert(light_control_readback_for_command(false,true,true,"","new",110,100));
    assert(light_control_readback_for_command(true,true,true,"new","new",10,UINT32_MAX-5));

    CodexEventGate gate;
    assert(!gate.accept("boot",4)); // No status baseline yet.
    gate.status("boot",4);
    assert(!gate.accept("boot",4)); // Retained historical approval.
    assert(gate.accept("boot",5));
    assert(!gate.accept("boot",5)); // QoS redelivery.
    gate.status("boot",6); // Normal status updates cannot eat the following event.
    assert(gate.accept("boot",6));
    gate.reconnect();
    assert(!gate.accept("boot",6));
    gate.status("boot",6);
    assert(!gate.accept("boot",6));
    assert(!gate.accept("other",7));
    gate.status("other",0);
    assert(gate.accept("other",1));
    std::puts("PASS: readback identity/order/freshness, offline, wrap, Codex replay gate");
}
