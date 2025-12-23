#include "stdbool.h"
#include "stdint.h"

typedef enum
{
    When_Tick_Ends_Restart = 1,
    When_Tick_Ends_Keep = 0,
} TickEndBehaviour;

typedef struct
{
    uint16_t ms_to_trigger;
    uint16_t ms_accumulated;
} Accumulator;

bool accumulator_tick(Accumulator *, float dt, TickEndBehaviour);
void accumulator_reset(Accumulator *);
