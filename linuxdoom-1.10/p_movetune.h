/* Movement tuning (F4 → Controls → Movement): multipliers for the player's
   walk and run speed, keyboard turning, momentum and view bob, 1 for
   unchanged (movewalk ... movebob, doomstat.h). Set from the settings; off in
   demos and netgames (P_MoveTuning), like the tuned movement, so recordings
   stay in sync. The math is here on its own so tests/move_tuning.c can check
   it without the game. */
#ifndef P_MOVETUNE_H
#define P_MOVETUNE_H

/* A movement amount (ticcmd move or turn) scaled, rounded to nearest. */
static inline int P_TunedMove (int move, double scale)
{
    return (int)(move*scale + (move < 0 ? -0.5 : 0.5));
}

/* Momentum m: the floor takes 1/m of the speed it took each tic, so the
   player takes about m times as long to speed up and to slow down. Friction
   is the 16.16 share of speed kept each tic. */
static inline int P_TunedFriction (int friction, double momentum)
{
    if (momentum <= 0 || momentum == 1)
	return friction;
    return 65536 - (int)((65536 - friction)/momentum + 0.5);
}

/* ...and the push from the keys shrinks by as much, so the top speed,
   push/(1 - friction), stays the same. */
static inline int P_TunedThrust (int thrust, double momentum)
{
    if (momentum <= 0 || momentum == 1)
	return thrust;
    return (int)(thrust/momentum + 0.5);
}

/* A turn key held for held tics: from the slow turn to the full one,
   evenly over ramp tics (at once with no ramp). */
static inline int P_TurnRamp (int slow, int full, int held, double ramp)
{
    double t = ramp > 0 ? held/ramp : 1;
    if (t > 1)
	t = 1;
    return slow + (int)((full - slow)*t + 0.5);
}

#endif
