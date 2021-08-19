#ifndef _TIMER_H_
#define _TIMER_H_

#include "kernel.h"
#include "utils.h"

#define ALPHA (1/8)
#define BETA (1/4)

// clock granularities
#define CLOCK_G (0.1)

typedef struct rtt_timer_t {
    float estimated_rtt;
    float dev_rtt;
    float timeout;
} rtt_timer_t;

// extern struct time_t* timers;

void timers_init(rtt_timer_t* timers);
void timer_init(rtt_timer_t* timer);
void timer_update(rtt_timer_t* timer, float rtt);

#endif