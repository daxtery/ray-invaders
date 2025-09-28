#include "accumulator.h"

bool accumulator_tick(Accumulator *accumulator, float dt, TickEndBehaviour when_tick_ends)
{
    if (accumulator->ms_accumulated > accumulator->ms_to_trigger)
    {
        if (when_tick_ends == When_Tick_Ends_Restart)
        {
            accumulator->ms_accumulated = 0;
        }
        return true;
    }

    float add = dt * 1000.0;
    accumulator->ms_accumulated += add;
    return false;
}
