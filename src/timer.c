#include "timer.h"

struct timer_t* timers[MAX_SOCK];

void timers_init(rtt_timer_t* timers) {
    for (int i = 0; i < MAX_SOCK; i++) {
        timer_init(&timers[i]);
    }
}

void timer_init(rtt_timer_t* timer) {
    timer->timeout = 1;
    timer->dev_rtt = 1;
    timer->estimated_rtt = 1;
}

void timer_update(rtt_timer_t* timer, float rtt) {
    timer->estimated_rtt = (1 - ALPHA) *  timer->estimated_rtt + ALPHA * rtt;
    timer->dev_rtt = (1 - BETA) * timer->dev_rtt + BETA * abs(timer->estimated_rtt - rtt);
    /*  From RFC6298
        RTO <- SRTT + max (G, K*RTTVAR)
        There is no requirement for the clock granularity G used for
        computing RTT measurements and the different state variables.
        However, if the K*RTTVAR term in the RTO calculation equals zero, the
        variance term MUST be rounded to G seconds (i.e., use the equation
        given in step 2.3).

        RTO <- SRTT + max (G, K*RTTVAR)

        Experience has shown that finer clock granularities (<= 100 msec)
        perform somewhat better than coarser granularities.
    */
    timer->timeout = timer->estimated_rtt + max(CLOCK_G, 4 * timer->dev_rtt);
}