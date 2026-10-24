#include <cassert>
#include <cstdio>
#include "services/AlwaysReadyPolicy.h"
using namespace AlwaysReady;
int main() {
    DarkTurnGate gate;
    assert(gate.sample(false,10,0)==NO_INTENT);
    assert(gate.sample(false,12,50)==NO_INTENT);  // entry rebase guard
    assert(gate.sample(false,13,200)==SWITCH_ON);
    assert(gate.sample(false,12,220)==SWITCH_OFF);
    assert(gate.sample(true,17,250)==NO_INTENT);   // pressing + shaft movement
    assert(gate.sample(true,19,390)==NO_INTENT);
    assert(gate.sample(false,22,410)==NO_INTENT);  // release movement
    assert(gate.sample(false,23,560)==NO_INTENT);
    assert(gate.sample(false,24,600)==SWITCH_ON);
    gate.reset(400,700);                          // motor mode rebase
    assert(gate.sample(false,1,710)==NO_INTENT);
    assert(gate.sample(false,2,900)==SWITCH_ON);
    AbsoluteSwitchTxn txn;
    assert(txn.should_send(SWITCH_ON,true,false,1000));
    txn.sent(SWITCH_ON,1000);
    assert(!txn.should_send(SWITCH_ON,true,false,1010));
    assert(txn.should_send(SWITCH_OFF,true,false,1011)); // opposite must not be dropped
    txn.sent(SWITCH_OFF,1011);
    assert(!txn.should_send(SWITCH_OFF,true,false,1012));
    txn.observed(true,false,1100);
    assert(!txn.pending());
    assert(txn.should_send(SWITCH_ON,true,false,1200));
    txn.sent(SWITCH_ON,1200);
    assert(txn.should_send(SWITCH_ON,false,false,6000)); // timeout retry on user action
    assert(!txn.should_send(NO_INTENT,false,false,6000));
    assert(Budget::kDarkSeconds > Budget::kDimSeconds);
    std::puts("PASS: dark-turn, press guard, motor rebase, flips, stale/pending, budget");
}
