#include <Arduino.h>

#include "driver/mcpwm_cap.h"
#include "esp_private/esp_clk.h"

#undef LISTEN_TO_PWM
#define LOG_AFTERWARD

#define PWM_PIN 2
#define TACH_PIN 4
#ifdef LISTEN_TO_PWM
  #define PWM_LOOP_PIN 6
#endif

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

#define CHECK_EQ(expr, expected) \
  (SUBSYS_CHECK_EQ(__func__, (expr), (expected)))
#define CHECK_NE(expr, fail_value) \
  (SUBSYS_CHECK_NE(__func__, (expr), (fail_value)))
#define CHECK_TRUE(expr) \
  (CHECK_NE((expr), false))
#define CHECK_NONNULL(expr) \
  (CHECK_NE((expr), NULL))

#define ESP_CHECK_TRUE(expr) \
  (SUBSYS_CHECK_NE("ESP", (expr), false))
#define ESP_CHECK_OK(expr) \
  (SUBSYS_CHECK_EQ("ESP", (expr), ESP_OK))

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
  pinMode(LED_BUILTIN, OUTPUT);
  uint32_t then = millis();
  while (1) {
    uint32_t now = millis();
    if ((now & 1023) == 0) {
      if (now != then) {
        Serial.printf("%s: error %d (%#x) at %s:%d\n",
                      subsystem, (int)err, (int)err, file, line);
        then = now;
      }
      digitalWrite(LED_BUILTIN, now / 256 & 1);
    }
  }
}


// DATA // ------ // ------ // ------ // ------ // ------ // ------ // ------ //

struct datum {
  const char *name;
  float value;
};
datum data[] = {
  { "Duty", 0.0 },
  { "Speed", 0.0 },
};
enum data_index {
  DATUM_DUTY = 0,
  DATUM_SPEED = 1,
  DATA_COUNT = 2
};
SemaphoreHandle_t data_mutex;

void update_datum(data_index i, float new_value) {
  RTOS_CHECK_TRUE(xSemaphoreTake(data_mutex, portMAX_DELAY));
  data[i].value = new_value;
  RTOS_CHECK_TRUE(xSemaphoreGive(data_mutex));
}

void get_data(float new_values[DATA_COUNT]) {
  RTOS_CHECK_TRUE(xSemaphoreTake(data_mutex, portMAX_DELAY));
  for (size_t i = 0; i < DATA_COUNT; i++) {
    new_values[i] = data[i].value;
  }
  RTOS_CHECK_TRUE(xSemaphoreGive(data_mutex));
}

#ifdef LOG_AFTERWARD

typedef uint32_t log_event_type;
const size_t LOG_MAX_SECONDS = 60;
const size_t LOG_MAX_PER_SECOND = 60000;
const size_t LOG_MAX_EVENTS = LOG_MAX_SECONDS * LOG_MAX_PER_SECOND;
size_t log_index = 0;
log_event_type *psram_log_data = NULL;

void store_value(uint32_t value) {
  if (log_index < LOG_MAX_EVENTS) {
    psram_log_data[log_index++] = value;
  }
}

void dump_log() {
  Serial.println();
  Serial.printf("%lz events logged\n", log_index);
  Serial.println();
}

#endif

void setup_data() {
  data_mutex = xSemaphoreCreateMutex();
  RTOS_CHECK_NONNULL(data_mutex);
#ifdef LOG_AFTERWARD
  psram_log_data = 
    (log_event_type *)ps_malloc(LOG_MAX_EVENTS * sizeof *psram_log_data);
#endif
}


// PWM  // ------ // ------ // ------ // ------ // ------ // ------ // ------ //

#define PWM_FREQUENCY 25000
//#define PWM_FREQUENCY 39
#define PWM_BITS 10
#define PWM_RESOLUTION (1 << PWM_BITS)

// #define SLOW_BOY 

void update_duty() {

#ifdef SLOW_BOY
  const int MAX_DUTY = 255;
  const int MIN_DUTY = 31;
  const int STEP_DUTY = 40;
#else
  const int MAX_DUTY = PWM_RESOLUTION - 1;
  const int MIN_DUTY = 0;
  const int STEP_DUTY = PWM_RESOLUTION / 3.6;
#endif

  static int dir = -1;
  static int duty = MAX_DUTY;

  duty += dir * STEP_DUTY;
  if (duty < MIN_DUTY) {
    duty = MIN_DUTY;
    dir = +1;
  } else if (duty > MAX_DUTY) {
    duty = MAX_DUTY;
    dir = -1;
  }
  // Serial.printf("duty %d/%d   \n", duty, MAX_DUTY);
  update_datum(DATUM_DUTY, (float)duty / (float)MAX_DUTY);
  CHECK_TRUE(ledcWrite(PWM_PIN, duty));
}

void pwm_change_duty_task_main(void *) {

  while (1) {
    update_duty();
    delay(3000);
  }
}

void setup_pwm() {
  pinMode(PWM_PIN, OUTPUT);
  CHECK_TRUE(ledcAttach(PWM_PIN, PWM_FREQUENCY, PWM_BITS));

  RTOS_CHECK_PASS(xTaskCreate(pwm_change_duty_task_main,
                              "PWM change duty task",
                              20480,
                              NULL,
                              1,
                              NULL));
}


// TACH // ------ // ------ // ------ // ------ // ------ // ------ // ------ //

// The tach callback can only pass 32 bits to baselevel task.
union tach_event {
  struct {
#ifdef LISTEN_TO_PWM
    uint32_t channel:1;
    uint32_t dir:1;
    uint32_t count:30;
#else
    uint32_t dir:1;
    uint32_t count:31;
#endif
  };
  uint32_t int_value;
};

mcpwm_cap_channel_handle_t tach_capture_channel;
#ifdef LISTEN_TO_PWM
mcpwm_cap_channel_handle_t pwm_capture_channel;
#endif

static bool
tach_capture_callback(mcpwm_cap_channel_handle_t channel,
                      const mcpwm_capture_event_data_t *edata,
                      void *user_data) {
  TaskHandle_t task_to_notify = (TaskHandle_t)user_data;
  BaseType_t high_task_woken = pdFALSE;

  tach_event evt;
#ifdef LISTEN_TO_PWM
  CHECK_TRUE(channel == tach_capture_channel || channel == pwm_capture_channel);
  evt.channel = channel == pwm_capture_channel;
#endif
  evt.dir = edata->cap_edge == MCPWM_CAP_EDGE_POS;
  evt.count = edata->cap_value;

  static int updown;
  updown = 1 - updown;
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, updown);

  RTOS_CHECK_PASS(xTaskNotifyFromISR(task_to_notify,
                                     evt.int_value,
                                     eSetValueWithOverwrite,
                                     &high_task_woken));

  return high_task_woken;
}

void tach_task_main(void *) {
#ifdef LISTEN_TO_PWM
  uint32_t prev_count[2][2] = {{0, 0}, {0, 0}};
  uint32_t rev_duration[2][2] = {{0, 0}, {0, 0}};
#else
  uint32_t prev_count[2] = {0, 0};
  uint32_t rev_duration[2] = {0, 0};
#endif
  const uint32_t CLK_FREQ = esp_clk_apb_freq();
  const uint32_t RIDICULOUS_DURATION = 1.0 * CLK_FREQ; // 1 second
  const float CLKS_to_USEC = 1.0e6 / CLK_FREQ;

  while (1) {
    tach_event evt;
    if (xTaskNotifyWait(0,
                        ULONG_MAX,
                        &evt.int_value,
                        pdMS_TO_TICKS(1000)) == pdTRUE) {
#ifdef LISTEN_TO_PWM
      size_t chan_index = evt.channel;
      size_t dir_index = evt.dir;
      uint32_t duration = (evt.count - prev_count[chan_index][dir_index]);
      duration &= (1 << 30) - 1;
      prev_count[chan_index][dir_index] = evt.count;
      rev_duration[chan_index][dir_index] = duration;
      Serial.printf("%s %d %d %d\n", __func__, evt.channel, evt.dir, evt.count);
#else
      size_t index = evt.dir;
      uint32_t duration = (evt.count - prev_count[index]);
      duration &= ((uint32_t)1 << 31) - 1;
      prev_count[index] = evt.count;
      rev_duration[index] = duration;
#endif
      if (duration < RIDICULOUS_DURATION) {
#ifdef LISTEN_TO_PWM
        float pos_us = rev_duration[0][MCPWM_CAP_EDGE_POS] * CLKS_to_USEC;
        float neg_us = rev_duration[0][MCPWM_CAP_EDGE_NEG] * CLKS_to_USEC;
        // Serial.printf("Tach: duration %6g %6g\r", pos_us, neg_us);
        if (evt.channel == 0) {
          float rpm = 60e6 / pos_us;
          const float MAX_RPM = 10000;
          update_datum(DATUM_SPEED, rpm / MAX_RPM);
        }
#else
        float pos_us = rev_duration[MCPWM_CAP_EDGE_POS] * CLKS_to_USEC;
        float neg_us = rev_duration[MCPWM_CAP_EDGE_NEG] * CLKS_to_USEC;
        // Serial.printf("Tach: duration %6g %6g\r", pos_us, neg_us);
        float rpm = 60e6 / pos_us;
        const float MAX_RPM = 10000;
        update_datum(DATUM_SPEED, rpm / MAX_RPM);
#endif
      } else {
        Serial.printf("ridiculous %d\n", duration);
      }
    } else {
      Serial.printf("%s: no event\n", __func__);
    }
  }
}

void setup_tach() {

  // Init capture timer
  mcpwm_cap_timer_handle_t cap_timer = NULL;
  mcpwm_capture_timer_config_t cap_conf = {
    .group_id = 0,
    .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
  };
  ESP_CHECK_OK(mcpwm_new_capture_timer(&cap_conf, &cap_timer));

  // Init tach capture channel
  mcpwm_capture_channel_config_t tach_cap_ch_conf = {
    .gpio_num = TACH_PIN,
    .prescale = 1,
    // .flags = { 1, 1, 1, 0, 0, 0, 0 },
  };
  tach_cap_ch_conf.flags.pos_edge = true;
  tach_cap_ch_conf.flags.neg_edge = true;
  tach_cap_ch_conf.flags.pull_up = true;
  ESP_CHECK_OK(mcpwm_new_capture_channel(cap_timer,
                                         &tach_cap_ch_conf,
                                         &tach_capture_channel));

#ifdef LISTEN_TO_PWM
  // Init pwm capture channel
  mcpwm_capture_channel_config_t pwm_cap_ch_conf = {
    .gpio_num = PWM_LOOP_PIN,
    .prescale = 1,
    // .flags = { 1, 1, 0, 0, 0, 0, 0 },
  };
  pwm_cap_ch_conf.flags.pos_edge = true;
  pwm_cap_ch_conf.flags.neg_edge = true;
  ESP_CHECK_OK(mcpwm_new_capture_channel(cap_timer,
                                         &pwm_cap_ch_conf,
                                         &pwm_capture_channel));
#endif

  // Create tach task
  TaskHandle_t tach_task = NULL;
  RTOS_CHECK_PASS(xTaskCreate(tach_task_main,
                              "Tach task",
                              2048,
                              NULL,
                              1,
                              &tach_task));


  // Register tach capture callback
  mcpwm_capture_event_callbacks_t callbacks = {
    .on_cap = tach_capture_callback,
  };
  ESP_CHECK_OK(
    mcpwm_capture_channel_register_event_callbacks(
      tach_capture_channel, &callbacks, tach_task));

#ifdef LISTEN_TO_PWM
  // Register pwm capture callback
  ESP_CHECK_OK(
    mcpwm_capture_channel_register_event_callbacks(
      pwm_capture_channel, &callbacks, tach_task));
#endif

  // Enable tach capture channel
  ESP_CHECK_OK(mcpwm_capture_channel_enable(tach_capture_channel));

#ifdef LISTEN_TO_PWM
  // Enable pwm capture channel
  ESP_CHECK_OK(mcpwm_capture_channel_enable(pwm_capture_channel));
#endif

  // Enable and start capture timer
  ESP_CHECK_OK(mcpwm_capture_timer_enable(cap_timer));
  ESP_CHECK_OK(mcpwm_capture_timer_start(cap_timer));
}


// PLOTTER // --- // ------ // ------ // ------ // ------ // ------ // ------ //

TaskHandle_t plotter_task;

void plotter_wakeup(TimerHandle_t) {
  RTOS_CHECK_TRUE(xTaskNotifyGive(plotter_task));
}

void plotter_update() {
  float values[DATA_COUNT];
  get_data(values);
#if 0
  const char *sep = "";
  for (size_t i = 0; i < DATA_COUNT; i++) {
    Serial.printf("%s%s:%g", sep, data[i].name, values[i]);
    sep = ",";
  }
  Serial.println();
#else
  Serial.printf("Duty:%g,Speed:%g\n", values[0], values[1]);
#endif
}

void plotter_main(void *) {
  while (1) {
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1001))) {
      plotter_update();
    }
  }
}

void setup_plotter() {
  RTOS_CHECK_PASS(xTaskCreate(plotter_main,
                  "plotter task",
                  2048,
                  NULL,
                  1,
                  &plotter_task));
  TimerHandle_t timer = xTimerCreate("plot interval",
                                     pdMS_TO_TICKS(999),
                                     pdTRUE,
                                     NULL,
                                     plotter_wakeup);
  RTOS_CHECK_NONNULL(timer);
  xTimerStart(timer, 0);
}


// ARDUINO // --- // ------ // ------ // ------ // ------ // ------ // ------ //

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.printf("Let's go!\n");

  setup_data();
  setup_pwm();
  setup_tach();
  setup_plotter();

  Serial.printf("heap  size %d\n", ESP.getHeapSize());
  Serial.printf("heap  free %d\n", ESP.getFreeHeap());
  Serial.printf("PSRAM size %d\n", ESP.getPsramSize());
  Serial.printf("PSRAM free %d\n", ESP.getFreePsram());
}

void loop() {}
