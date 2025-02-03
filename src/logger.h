#if 0

#ifndef LOGGER_included
#define LOGGER_included

#include "check_macros.h"

class Logger {
public:

    typedef uint32_t event_type;
    const size_t MAX_SECONDS = 30;
    const size_t MAX_PER_SECOND = 60000;
    const size_t MAX_EVENTS = MAX_SECONDS * MAX_PER_SECOND;

    static log_event_type *psram_log_data = NULL;
    static volatile size_t log_index = 0;
    static volatile bool logging_enabled;

    static void enable_logging() {
        log_index = 0;
        logging_enabled = true;
        } interrupts();
    }

    void disable_logging() {
        noInterrupts(); {
            logging_enabled = false;
        } interrupts();
    }

    inline bool log_value_from_ISR(uint32_t value) {
        noInterrupts();
        if (logging_enabled && log_index < LOG_MAX_EVENTS) {
            psram_log_data[log_index++] = value;
    return true;
  }
  return false;
}

void log_value(uint32_t value) {
  RTOS_CHECK_TRUE(xSemaphoreTake(data_mutex, portMAX_DELAY));
  log_value_no_lock(value);
  RTOS_CHECK_TRUE(xSemaphoreGive(data_mutex));
}

    static void dump_log() {
        CHECK_EQ(logging_enabled, false);
        printf("\n%lu events logged\n\n", (uint32_t)log_index);
        for (size_t i = 0; i < log_index; i++) {
            printf("%x%c", psram_log_data[i], ((i % 8) == 7) ? '\n' : ' ');
            if (i % 8 == 7) { delay(1); }
        }
        printf("\n\n");
    }

    static volatile 
    static void setup() {
        psram_log_data = calloc(MAX_EVENTS, sizeof *psram_log_data);
        CHECK_NONNULL(psram_log_data);
    }
};

#endif /* !LOGGER_included */

#endif
