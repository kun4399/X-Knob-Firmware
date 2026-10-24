#include <cassert>
#include <cstdio>
#include "services/GestureFSM.h"

int main() {
    GestureFSM g;
    g.update(false, 0, 0);
    // Hard press begins moving rotor before debounce completes.
    assert(g.update(true, 1, 10).kind == GestureFSM::NONE);
    assert(g.update(true, 2, 45).kind == GestureFSM::NONE);
    assert(g.update(true, 3, 190).kind == GestureFSM::NONE); // press guard
    assert(g.update(true, 4, 230).kind == GestureFSM::NONE); // still <3 intentional steps
    g.update(false, 5, 270);
    assert(g.update(false, 6, 300).kind == GestureFSM::NONE);
    assert(g.update(false, 7, 440).kind == GestureFSM::NONE);
    assert(g.update(false, 7, 800).kind == GestureFSM::SINGLE_CLICK);
    assert(g.update(false, 8, 820).kind == GestureFSM::ROTATE);

    GestureFSM intentional;
    intentional.update(false,0,0);
    intentional.update(true,0,10); intentional.update(true,0,40);
    assert(intentional.update(true,1,240).kind == GestureFSM::NONE);
    auto hold=intentional.update(true,3,270);
    assert(hold.kind == GestureFSM::HOLD_ROTATE && hold.delta == 3);
    assert(intentional.update(true,3,1400).kind == GestureFSM::NONE);
    intentional.update(false,3,1450); intentional.update(false,3,1500);
    assert(intentional.update(false,3,2000).kind == GestureFSM::NONE);

    GestureFSM long_press;
    long_press.update(false,0,0);
    long_press.update(true,0,10); long_press.update(true,0,40);
    assert(long_press.update(true,0,1240).kind == GestureFSM::LONG_PRESS);

    GestureFSM double_press;
    double_press.update(false,0,0);
    double_press.update(true,0,10);double_press.update(true,0,40);
    double_press.update(false,0,100); double_press.update(false,0,130);
    double_press.update(true,0,170);double_press.update(true,0,200);
    double_press.update(false,0,250);
    assert(double_press.update(false,0,280).kind == GestureFSM::DOUBLE_CLICK);
    std::puts("PASS: press-induced movement, hold intent threshold, click, double, long");

    // Preserve the pre-v3 click, wake and parent-page regressions.
    {
    GestureFSM g;
    g.update(false, 0, 0);
    g.update(true, 0, 10);
    g.update(true, 0, 40);
    g.update(false, 0, 100);
    g.update(false, 0, 130);
    assert(g.update(false, 0, 551).kind == GestureFSM::SINGLE_CLICK);

    // Mechanical shaft movement during a button click must be consumed by the
    // gesture arbiter instead of leaking into a brightness adjustment.
    GestureFSM click_with_shaft_motion;
    click_with_shaft_motion.update(false, 0, 0);
    click_with_shaft_motion.update(true, 0, 10);
    click_with_shaft_motion.update(true, 0, 40);
    assert(click_with_shaft_motion.update(true, 1, 80).kind == GestureFSM::NONE);
    click_with_shaft_motion.update(false, 1, 100);
    assert(click_with_shaft_motion.update(false, 1, 130).kind == GestureFSM::NONE);
    assert(click_with_shaft_motion.update(false, 1, 551).kind == GestureFSM::SINGLE_CLICK);

    GestureFSM d;
    d.update(false, 0, 0);
    d.update(true, 0, 10); d.update(true, 0, 40);
    d.update(false, 0, 70); d.update(false, 0, 100);
    d.update(true, 0, 150); d.update(true, 0, 180);
    d.update(false, 0, 210);
    assert(d.update(false, 0, 240).kind == GestureFSM::DOUBLE_CLICK);

    GestureFSM h;
    h.update(false, 0, 0);
    h.update(true, 0, 10); h.update(true, 0, 40);
    assert(h.update(true, 3, 240).kind == GestureFSM::HOLD_ROTATE);
    assert(h.update(true, 3, 1240).kind == GestureFSM::NONE);
    h.update(false, 3, 1300);
    assert(h.update(false, 3, 1330).kind == GestureFSM::NONE);

    // A turn arriving on the long-press boundary wins over a stationary
    // long-press event and suppresses the click/long action for that hold.
    GestureFSM hold_and_turn_at_threshold;
    hold_and_turn_at_threshold.update(false, 0, 0);
    hold_and_turn_at_threshold.update(true, 0, 10);
    hold_and_turn_at_threshold.update(true, 0, 40);
    assert(hold_and_turn_at_threshold.update(true, 0, 1230).kind == GestureFSM::NONE);
    GestureFSM::Event boundary_turn = hold_and_turn_at_threshold.update(true, 3, 1240);
    assert(boundary_turn.kind == GestureFSM::HOLD_ROTATE && boundary_turn.delta == 3);
    hold_and_turn_at_threshold.update(false, 3, 1300);
    assert(hold_and_turn_at_threshold.update(false, 3, 1330).kind == GestureFSM::NONE);

    GestureFSM wake;
    wake.update(false, 0, 0);
    wake.consume_wake_press(true);
    wake.update(true, 0, 10); wake.update(true, 0, 40);
    assert(wake.update(true, 4, 80).kind == GestureFSM::NONE);
    wake.update(false, 4, 120);
    assert(wake.update(false, 4, 150).kind == GestureFSM::NONE);
    assert(wake.update(false, 4, 500).kind == GestureFSM::NONE);

    // If the wake press has already been released before its flag is handled,
    // it must not suppress the next real click.
    GestureFSM released_wake;
    released_wake.update(false, 0, 0);
    released_wake.consume_wake_press(false);
    released_wake.update(true, 0, 10); released_wake.update(true, 0, 40);
    released_wake.update(false, 0, 100); released_wake.update(false, 0, 130);
    assert(released_wake.update(false, 0, 551).kind == GestureFSM::SINGLE_CLICK);

    GestureFSM long_press;
    long_press.update(false, 0, 0);
    long_press.update(true, 0, 10); long_press.update(true, 0, 40);
    assert(long_press.update(true, 0, 1240).kind == GestureFSM::LONG_PRESS);
    long_press.update(false, 0, 1300);
    assert(long_press.update(false, 0, 1330).kind == GestureFSM::NONE);
    // A long hold consumed by a child page cannot become a click on its parent.
    long_press.suppress_pressed_until_release(true);
    long_press.update(true, 0, 1400); long_press.update(true, 0, 1430);
    assert(long_press.update(true, 0, 2630).kind == GestureFSM::NONE);
    long_press.update(false, 0, 2680); long_press.update(false, 0, 2710);
    assert(long_press.update(false, 0, 3200).kind == GestureFSM::NONE);

    // Returning to a parent while the child's long press is still held must
    // suppress the release and must never emit a short click afterward.
    GestureFSM returned;
    returned.update(false, 0, 0);
    returned.suppress_pressed_until_release(true);
    returned.update(true, 0, 10);
    returned.update(true, 0, 40);
    returned.update(false, 0, 100);
    returned.update(false, 0, 130);
    assert(returned.update(false, 0, 600).kind == GestureFSM::NONE);
    }
}
