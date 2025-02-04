#pragma once

// #include <Arduino.h>

#include "freertos/FreeRTOS.h"

#include "driver/mcpwm_cap.h"

#include "check_macros.h"
#include "trace_buffer.h"

template <BaseType_t CORE, gpio_num_t PWM_IN_PIN, gpio_num_t TACH_PIN>
class Capture {

public:
    void setup() {
        RTOS_CHECK_PASS(
            xTaskCreatePinnedToCore(
                task_main,
                "capture",
                2048,
                this,
                2,
                &task_handle,
                CORE));
    }

private:
    mcpwm_cap_channel_handle_t tach_capture_channel = NULL;
    mcpwm_cap_channel_handle_t pwm_up_capture_channel = NULL;
    mcpwm_cap_channel_handle_t pwm_dn_capture_channel = NULL;
    TaskHandle_t task_handle = NULL;

    void run() {
        // Init capture timer
        mcpwm_cap_timer_handle_t cap_timer = NULL;
        mcpwm_capture_timer_config_t cap_conf = {
            .group_id = 0,
            .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
            .resolution_hz = 0,
            .flags = {
                .allow_pd = false,
            },
        };
        ESP_CHECK_OK(mcpwm_new_capture_timer(&cap_conf, &cap_timer));

        // Init tach capture channel
        mcpwm_capture_channel_config_t tach_cap_ch_conf = {
            .gpio_num = TACH_PIN,
            .intr_priority = 1,
            .prescale = 1,
            .flags = {
                .pos_edge = true,
                .neg_edge = true,
                .pull_up = true,
                .pull_down = false,
                .invert_cap_signal = false,
                .io_loop_back = false,
                .keep_io_conf_at_exit = false,
            },
        };
        ESP_CHECK_OK(mcpwm_new_capture_channel(cap_timer,
                                               &tach_cap_ch_conf,
                                               &tach_capture_channel));

        // Register tach capture callback
        mcpwm_capture_event_callbacks_t callbacks = {
            .on_cap = capture_callback,
        };
        ESP_CHECK_OK(
            mcpwm_capture_channel_register_event_callbacks(
                tach_capture_channel, &callbacks, this));

        // Init pwm rising edge capture channel
        mcpwm_capture_channel_config_t pwm_up_cap_ch_conf = {
            .gpio_num = PWM_IN_PIN,
            .intr_priority = 1,
            .prescale = 1,
            .flags = {
                .pos_edge = true,
                .neg_edge = false,
                .pull_up = false,
                .pull_down = false,
                .invert_cap_signal = false,
                .io_loop_back = false,
                .keep_io_conf_at_exit = false,
            },
        };
        ESP_CHECK_OK(mcpwm_new_capture_channel(
                     cap_timer,
                     &pwm_up_cap_ch_conf,
                     &pwm_up_capture_channel));
        
        ESP_CHECK_OK(
            mcpwm_capture_channel_register_event_callbacks(
                pwm_up_capture_channel, &callbacks, this));

        // Init pwm falling edge capture channel
        mcpwm_capture_channel_config_t pwm_dn_cap_ch_conf = {
            .gpio_num = PWM_IN_PIN,
            .intr_priority = 1,
            .prescale = 1,
            .flags = {
                .pos_edge = false,
                .neg_edge = true,
                .pull_up = false,
                .pull_down = false,
                .invert_cap_signal = false,
                .io_loop_back = false,
                .keep_io_conf_at_exit = false,
            },
        };
        ESP_CHECK_OK(mcpwm_new_capture_channel(
                     cap_timer,
                     &pwm_dn_cap_ch_conf,
                     &pwm_dn_capture_channel));
        
        ESP_CHECK_OK(
            mcpwm_capture_channel_register_event_callbacks(
                pwm_dn_capture_channel, &callbacks, this));

        // Enable tach capture channel
        ESP_CHECK_OK(mcpwm_capture_channel_enable(tach_capture_channel));

        // Enable pwm capture channels
        ESP_CHECK_OK(mcpwm_capture_channel_enable(pwm_up_capture_channel));
        ESP_CHECK_OK(mcpwm_capture_channel_enable(pwm_dn_capture_channel));

        // Enable and start capture timer
        ESP_CHECK_OK(mcpwm_capture_timer_enable(cap_timer));
        ESP_CHECK_OK(mcpwm_capture_timer_start(cap_timer));

        while (1) {
            vTaskDelay(portMAX_DELAY);
        }
    }

    static void task_main(void *user_data) {
        Capture *cap = static_cast<Capture *>(user_data);
        cap->run();
    }

    static bool
    capture_callback(mcpwm_cap_channel_handle_t cap_channel,
                     const mcpwm_capture_event_data_t *edata,
                     void *user_data) {
        Capture *cap = static_cast<Capture *>(user_data);

        assert(edata->cap_edge == MCPWM_CAP_EDGE_POS ||
               edata->cap_edge == MCPWM_CAP_EDGE_NEG);
        uint8_t chan = cap_channel != cap->tach_capture_channel;
        uint8_t dir = edata->cap_edge == MCPWM_CAP_EDGE_POS;
        uint32_t timestamp = edata->cap_value;
        TraceEvent evt(chan, dir, timestamp);
        TraceBuffer::record_trace_from_ISR(evt);
        return false;
    }
};
