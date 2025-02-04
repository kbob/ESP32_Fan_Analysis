#pragma once

// #include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>

#include "esp_heap_caps.h"
#include "esp_psram.h"

#include "check_macros.h"

struct TraceEvent {
    uint32_t int_value;

    TraceEvent(uint8_t channel, uint8_t dir, uint32_t timestamp)
    : int_value(encode(channel, dir, timestamp)) {}

    explicit TraceEvent(uint32_t value)
    : int_value(value) {}

    TraceEvent(const TraceEvent&) = default;
    TraceEvent& operator = (const TraceEvent&) = default;

    static uint32_t encode(uint8_t channel, uint8_t dir, uint32_t timestamp) {
        return (channel & 1) | ((dir & 1) << 1) | (timestamp << 2);
    }

    uint8_t channel() const {
        return int_value & 1;
    }

    uint8_t dir() const {
        return int_value >> 1 & 1;
    }

    uint32_t timestamp() const {
        return int_value >> 2;
    }
};

class TraceBuffer {
public:
    static const size_t MAX_TRACES_PER_SECOND = 52'000;
    static const size_t MAX_SECONDS = 30;
    static const size_t MAX_TRACES = MAX_TRACES_PER_SECOND * MAX_SECONDS;

    static portMUX_TYPE spinlock;
    static bool tracing_enabled;
    static size_t trace_index;
    static TraceEvent *psram_trace_buffer;

    static void begin_recording() {
        taskENTER_CRITICAL(&spinlock);
        {
            trace_index = 0;
            tracing_enabled = true;
        }
        taskEXIT_CRITICAL(&spinlock);
    }

    static void end_recording() {
        taskENTER_CRITICAL(&spinlock);
        {
            tracing_enabled = false;
        }
        taskEXIT_CRITICAL(&spinlock);
    }

    static void
    inline
    record_trace_from_ISR(const TraceEvent evt) {
        // taskENTER_CRITICAL_ISR(&spinlock);
        {
            if (tracing_enabled &&trace_index < MAX_TRACES) {
                psram_trace_buffer[trace_index++] = evt;
            }
        }
        // taskEXIT_CRITICAL_ISR(&spinlock);
    }

    static void dump_trace_buffer(const char *title) {
        printf("\n%s\n", title);
        printf("\n%d events logged\n", trace_index);
        for (size_t i = 0; i < trace_index; i++) {
            printf("%lx%c",
                   psram_trace_buffer[i].int_value,
                   ((i % 8) == 7) ? '\n' : ' ');
            if (i % 16 == 15) {
                vTaskDelay(1);
            }
        }
        printf("\n\n");

    }

    static void setup() {
        tracing_enabled = false;
        psram_trace_buffer =
            (TraceEvent *)
                heap_caps_calloc(MAX_TRACES,
                                 sizeof *psram_trace_buffer,
                                 MALLOC_CAP_SPIRAM);
        CHECK_NONNULL(psram_trace_buffer);
        trace_index = 0;
    }
};
