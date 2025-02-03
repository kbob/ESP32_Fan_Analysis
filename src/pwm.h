#pragma once

#include <freertos/FreeRTOS.h>

#include "esp_clk_tree.h"
#include "esp_intr_alloc.h"
#include "driver/ledc.h"

#include "check_macros.h"
#include "trace_buffer.h"

enum Script {
    NO_SCRIPT,
    FULL_SPEED,
    HALF_SPEED,
    BANG_BANG,
    RAMPS,
    STAIRCASE,
};

extern const char *script_name(Script);

template <BaseType_t CORE, gpio_num_t PWM_PIN>
class PWM {
public:
    
    static const uint32_t PWM_FREQ = 25'000;

    void setup() {
        RTOS_CHECK_PASS(xTaskCreatePinnedToCore(
            task_main,
            "pwm",
            20480,
            this,
            2,
            &task_handle,
            CORE));
    }

    void run_script(Script s) {
        RTOS_CHECK_PASS(
            xTaskNotify(
                task_handle,
                static_cast<uint32_t>(s),
                eSetValueWithoutOverwrite));
    }

private:

    TaskHandle_t task_handle = NULL;
    uint32_t resolution_bits = 0;

    void run() {
        // Find best timer resolution
        uint32_t apb_freq_hz;
        ESP_CHECK_OK(
            esp_clk_tree_src_get_freq_hz(
                SOC_MOD_CLK_APB,
                ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT,
                &apb_freq_hz));
        resolution_bits =
            ledc_find_suitable_duty_resolution(apb_freq_hz, PWM_FREQ);

        // Configure a timer
        ledc_timer_config_t timer_config = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = static_cast<ledc_timer_bit_t>(resolution_bits),
            .timer_num = LEDC_TIMER_0,
            .freq_hz = PWM_FREQ,
            .clk_cfg = LEDC_USE_APB_CLK,
            .deconfigure = false,
        };
        ESP_CHECK_OK(ledc_timer_config(&timer_config));

        // Configure a channel
        ledc_channel_config_t channel_config = {
            .gpio_num = PWM_PIN,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 410, // 2048 * 20%
            .hpoint = 0,
            .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
            .flags = {
                .output_invert = false,
            },
        };
        ESP_CHECK_OK(ledc_channel_config(&channel_config));

        // Install a fade function
        ESP_CHECK_OK(ledc_fade_func_install(ESP_INTR_FLAG_LEVEL1));

        while (1) {
#undef FANCY_NONSENSE
#ifdef FANCY_NONSENSE
            fade_to(1.0, 5000);
            hold_current(500);
            hold_at(0.25, 500);
            hold_at(0.9, 500);
            hold_at(0.25, 500);
            hold_at(1.0, 500);
            fade_to(0.0, 5000);
            hold_current(1000);
#else
            hold_at(0.0, 0);
            uint32_t notification_value;
            RTOS_CHECK_PASS(
                xTaskNotifyWait(
                    0,
                    UINT32_MAX,
                    &notification_value,
                    portMAX_DELAY));
            Script s = static_cast<Script>(notification_value);
            printf("received script %lu\n", notification_value);
            do_run_script(s);
#endif
        }
    }

    void do_run_script(Script s);

    void fade_to(float level, uint32_t msec) {
        CHECK_TRUE(0.0 <= level && level <= 1.0);
        uint32_t target_duty = (1 << resolution_bits) * level;
        ESP_CHECK_OK(
            ledc_set_fade_time_and_start(
                LEDC_LOW_SPEED_MODE,
                LEDC_CHANNEL_0,
                target_duty,
                msec,
                LEDC_FADE_WAIT_DONE));
    }

    void hold_at(float level, uint32_t msec) {
        CHECK_TRUE(0.0 <= level && level <= 1.0);
        uint32_t target_duty = (1 << resolution_bits) * level;
        ESP_CHECK_OK(
            ledc_set_duty_and_update(
                LEDC_LOW_SPEED_MODE,
                LEDC_CHANNEL_0,
                target_duty,
                0));
        if (msec) {
            vTaskDelay(pdMS_TO_TICKS(msec));
        }
    }

    void hold_current(uint32_t msec) {
        vTaskDelay(pdMS_TO_TICKS(msec));
    }

    void begin_recording() {
        printf("begin recording\n");
        vTaskDelay(pdMS_TO_TICKS(1));
        TraceBuffer::begin_recording();
    }

    void end_recording() {
        TraceBuffer::end_recording();
        printf("end recording\n");
    }

    static void task_main(void *user_data) {
        PWM *pwm = static_cast<PWM *>(user_data);
        pwm->run();
    }
};

template<BaseType_t CORE, gpio_num_t PWM_PIN>
void
PWM<CORE, PWM_PIN>::do_run_script(Script script) {
    const uint32_t settling_msec = 3000;
    switch (script) {

    case NO_SCRIPT:
        break;

    case FULL_SPEED:
        hold_at(1.0, settling_msec);
        begin_recording();
        hold_current(3000);
        end_recording();
        break;

    case HALF_SPEED:
        hold_at(0.5, settling_msec);
        begin_recording();
        hold_current(3000);
        end_recording();
        break;

    case BANG_BANG:
        hold_at(0.0, settling_msec);
        begin_recording();
        hold_current(1000);
        hold_at(1.0, 5000);
        hold_at(0.0, 5000);
        end_recording();
        break;

    case RAMPS:
        hold_at(0.0, settling_msec);
        begin_recording();
        fade_to(1.0, 5000);
        fade_to(0.0, 5000);
        hold_current(settling_msec);
        end_recording();
        break;

    case STAIRCASE:
        hold_at(0.0, settling_msec);
        begin_recording();
        for (int i = 0; i <= 8; i++) {
            hold_at(i / 8.0, 1600);
        }
        for (int i = 7; i >= 0; --i) {
            hold_at(i / 8.0, 1600);
        }
        hold_current(settling_msec);
        end_recording();
        break;
    }
    hold_at(0.0, 0);
    TraceBuffer::dump_trace_buffer(script_name(script));
}
