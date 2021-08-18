#include "timer.h"

float max(float a, float b) {
    return a > b ? a : b;
}

void timer_init(timer_t* timer) {
    timer->rt0 = 0;
    timer->rttvar = 0;
    timer->srtt = 0;
}

void timer_update(timer_t* timer, float r) {
    timer->rttvar = (1 - BETA) * timer->rttvar + BETA * abs(timer->srtt - r);
    timer->srtt = (1 - ALPHA) *  timer->srtt + ALPHA * r;
    timer->rt0 = timer->srtt + max(3, 4 * timer->rttvar);
}