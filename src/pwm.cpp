#include "pwm.h"

const char *script_name(Script s) {
    switch (s) {

    case NO_SCRIPT:
        return "No scenario";

    case FULL_SPEED:
        return "Full speed scenario";

    case HALF_SPEED:
        return "Half speed scenario";

    case BANG_BANG:
        return "Bang-bang scenario";

    case RAMPS:
        return "Ramps scenario";

    case STAIRCASE:
        return "Staircase scenario";

    default:
        CHECK_TRUE(false);
        return NULL;
    }
}