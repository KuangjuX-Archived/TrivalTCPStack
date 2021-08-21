#ifndef _TIMER_H_
#define _TIMER_H_

#include "kernel.h"
#include "utils.h"

#define ALPHA (1/8)
#define BETA (1/4)
#define IMPROVE_ACK_TIME 200

// clock granularities
#define CLOCK_G (0.1)

typedef struct {
    float estimated_rtt;
    float dev_rtt;
    float timeout;
} timer_t;

extern time_t* timers;

#endif