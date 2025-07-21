#ifdef LOGGER_MAIN

// #include <Arduino.h>

#include <stdio.h>

#include <freertos/FreeRTOS.h>

#include "esp_heap_caps.h"
#include "esp_intr_alloc.h"
#include "esp_private/esp_clk.h"
#include "driver/gpio.h"

#include "capture.h"
#include "check_macros.h"
#include "pwm.h"
#include "trace_buffer.h"

const Script THE_SCRIPT = Script::STAIRCASE;

#define PWM_PIN          GPIO_NUM_2
#define TACH_PIN         GPIO_NUM_4
#define PWM_LOOPBACK_PIN GPIO_NUM_6

#define CAPTURE_CORE 1
#define PWM_CORE 0

PWM<PWM_CORE, PWM_PIN> pwm;
Capture<CAPTURE_CORE, PWM_LOOPBACK_PIN, TACH_PIN> capture;

void print_facts() {
    printf("CPU clock %d Hz\n", esp_clk_cpu_freq());
    printf("APB clock %d Hz\n", esp_clk_apb_freq());
    // printf("setup core id %d\n", xPortGetCoreID());
    // heap_caps_dump_all();
    // esp_intr_dump(stdout);
    // uint64_t pins = (1 << PWM_PIN) | (1 << TACH_PIN);
    // ESP_CHECK_OK(gpio_dump_io_configuration(stdout, pins));

    // // Print task list
    // size_t task_count = uxTaskGetNumberOfTasks();
    // char buffer[60 * task_count];
    // vTaskList(buffer);
    // printf("Name            State\tPrio\tStack\tNum\tCore\r\n");
    // printf("----            -----\t----\t-----\t---\t----\r\n");
    // printf("%s\n", buffer);
    // printf("\n");

}

void setup() {
    vTaskDelay(pdMS_TO_TICKS(5000));
    printf("Here we go!\n");
    TraceBuffer::setup();
    pwm.setup();
    vTaskDelay(pdMS_TO_TICKS(100));
    capture.setup();
    vTaskDelay(pdMS_TO_TICKS(20));
    print_facts();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Pause to read the messages
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Just once, run the script
    static bool been_here;
    if (!been_here) {
        pwm.run_script(THE_SCRIPT);
        vTaskDelay(pdMS_TO_TICKS(30'000));
        been_here = true;
    }
}

#endif
