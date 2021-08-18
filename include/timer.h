#ifndef _TIMER_H_
#define _TIMER_H_

#include "kernel.h"

#define ALPHA (1/8)
#define BETA (1/4)

typedef struct {
    float srtt;
    float rttvar;
    float rt0;
} timer_t;

timer_t* timers[MAX_SOCK];

#endif