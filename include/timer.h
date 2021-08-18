#ifndef _TIMER_H_
#define _TIMER_H_

#include <sys/types.h>
#include "kernel.h"

typedef struct {
    uint32_t srtt;
    uint32_t rttvar;
    uint32_t rt0;
} timer_t;

timer_t* timers[MAX_SOCK];

#endif