/* Movement tuning (p_movetune.h): momentum stretches starting and stopping but
   keeps the top speed, in both movement modes' frictions; scaling moves rounds
   both ways alike; 1 changes nothing. */
#include <stdio.h>
#include <stdlib.h>
#include "p_movetune.h"

static int failures;
#define CHECK(ok, ...) do { if (!(ok)) { ++failures; fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* Holding a key from a standstill, as P_MovePlayer and P_XYMovement do each
   tic: push, move that far, then keep friction's share. The top speed moved
   (push/(1 - friction)), and the tics to 90% of it. */
static void run_up(int thrust, int friction, double *top, int *tics)
{
    double v = 0, last = -1;
    int t = 0, reached = -1;
    for (t = 0; t < 2000 && v != last; ++t) {
        last = v;
        v = (v + thrust)*friction/65536.0;
    }
    *top = v + thrust;
    v = 0;
    for (t = 0; t < 2000; ++t) {
        if (reached < 0 && v + thrust >= 0.9*(*top)) reached = t + 1;
        v = (v + thrust)*friction/65536.0;
    }
    *tics = reached;
}

/* Letting go: tics until under DOOM's STOPSPEED (0x1000), from speed v. */
static int stop(double v, int friction)
{
    int t = 0;
    while (v >= 0x1000 && t < 2000) { v = v*friction/65536.0; ++t; }
    return t;
}

int main(void)
{
    /* The tuned movement's (key held, let go) and DOOM's own frictions and pushes. */
    const int held[] = {0xc000, 0xe800}, free_[] = {0xa000, 0xe800}, push[] = {2048*8/3*50, 2048*50};
    for (int mode = 0; mode < 2; ++mode) {
        double top1; int up1;
        run_up(push[mode], held[mode], &top1, &up1);
        int stop1 = stop(top1, free_[mode]);
        CHECK(P_TunedFriction(held[mode], 1) == held[mode] && P_TunedThrust(push[mode], 1) == push[mode],
              "mode %d: momentum 1 changes the movement", mode);
        for (double m = 0.5; m <= 3.0; m += 0.25) {
            double top; int up;
            run_up(P_TunedThrust(push[mode], m), P_TunedFriction(held[mode], m), &top, &up);
            int stopped = stop(top, P_TunedFriction(free_[mode], m));
            CHECK(top > top1*0.99 && top < top1*1.01, "mode %d momentum %.2f: top speed %.0f, not %.0f", mode, m, top, top1);
            if (m > 1) {
                CHECK(up > up1 && stopped > stop1, "mode %d momentum %.2f: %d tics up, %d to stop (1.00: %d, %d)",
                      mode, m, up, stopped, up1, stop1);
                CHECK(up < up1*m*1.6 + 2, "mode %d momentum %.2f: %d tics up is far over %.2f times %d", mode, m, up, m, up1);
            }
            if (m < 1) CHECK(up <= up1 && stopped <= stop1, "mode %d momentum %.2f: not quicker", mode, m);
        }
    }
    CHECK(P_TunedMove(25, 0.5) == 13 && P_TunedMove(-25, 0.5) == -13, "walk at half rounds unevenly");
    CHECK(P_TunedMove(50, 1) == 50 && P_TunedMove(-640, 1) == -640, "scale 1 changes the move");
    CHECK(P_TunedMove(50, 1.5) == 75, "run at 1.5 is not 75");
    /* Turn ramp: DOOM's slow 320 and running 1280 turns, over 6 tics times 0, 1 and 4. */
    CHECK(P_TurnRamp(320, 1280, 1, 0) == 1280, "no ramp is not full speed at once");
    CHECK(P_TurnRamp(320, 1280, 0, 24) == 320, "a ramp does not start at the slow turn");
    CHECK(P_TurnRamp(320, 1280, 12, 24) == 800, "halfway is %d, not 800", P_TurnRamp(320, 1280, 12, 24));
    CHECK(P_TurnRamp(320, 1280, 24, 24) == 1280 && P_TurnRamp(320, 1280, 99, 24) == 1280, "the ramp's end is not full speed");
    for (int held = 1, last = 0; held <= 30; ++held) {
        int turn = P_TurnRamp(320, 640, held, 24);
        CHECK(turn >= last, "the walking ramp turns slower at tic %d", held);
        last = turn;
    }
    if (failures) { fprintf(stderr, "%d failures\n", failures); return 1; }
    puts("move tuning: ok");
    return 0;
}
