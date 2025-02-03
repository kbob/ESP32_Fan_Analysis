#pragma once

#include <stdint.h>

#include "esp_timer.h"
#include "driver/gpio.h"

#define SUBSYS_CHECK_EQ(subsys, expr, expected) ({           \
  auto x##__LINE__ = (expr);                                 \
  if (x##__LINE__ != (expected)) {                           \
    report_error(x##__LINE__, (subsys), __FILE__, __LINE__); \
  }                                                          \
})

#define SUBSYS_CHECK_NE(subsys, expr, fail_value) ({         \
  auto x##__LINE__ = (expr);                                 \
  if (x##__LINE__ == (fail_value)) {                         \
    report_error(x##__LINE__, (subsys), __FILE__, __LINE__); \
  }                                                          \
})

// Generic check macros use calling function as subsystem
#define CHECK_EQ(expr, expected) \
  (SUBSYS_CHECK_EQ(__func__, (expr), (expected)))
#define CHECK_NE(expr, fail_value) \
  (SUBSYS_CHECK_NE(__func__, (expr), (fail_value)))
#define CHECK_TRUE(expr) \
  (CHECK_NE((expr), false))
#define CHECK_NONNULL(expr) \
  (CHECK_NE((expr), NULL))

// ESP (Espressif) macros
#define ESP_CHECK_TRUE(expr) \
  (SUBSYS_CHECK_NE("ESP", (expr), false))
#define ESP_CHECK_OK(expr) \
  (SUBSYS_CHECK_EQ("ESP", (expr), ESP_OK))

// FreeRTOS macros
#define RTOS_CHECK_TRUE(expr) \
  (SUBSYS_CHECK_EQ("ESP", (expr), pdTRUE))
#define RTOS_CHECK_PASS(expr) \
  (SUBSYS_CHECK_EQ("RTOS", (expr), pdPASS))
#define RTOS_CHECK_NONNULL(expr) \
  (SUBSYS_CHECK_NE("RTOS", (expr), NULL))

template <typename err_type>
void report_error(err_type err,
                  const char *subsystem,
                  const char *file,
                  int line) {
#define LED_BUILTIN 21
  // pinMode(LED_BUILTIN, OUTPUT);
  gpio_config_t led_config = {
    .pin_bit_mask = 1 << LED_BUILTIN,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  (void)gpio_config(&led_config);
  uint32_t then = esp_timer_get_time() >> 10;
  while (1) {
    uint32_t now = esp_timer_get_time() >> 10;
    if ((now & 1023) == 0) {
      if (now != then) {
        printf("%s: error %d (%#x) at %s:%d\n",
               subsystem, (int)err, (int)err, file, line);
        then = now;
      }
    }
    // digitalWrite(LED_BUILTIN, now / 256 & 1);
    (void)gpio_set_level(GPIO_NUM_21, now / 256 & 1);
  }
}
