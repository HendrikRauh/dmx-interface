/**
 * @file led.c
 * @brief Implementation of LEDC-driven status LED with dynamic generator.
 */

#define LOG_TAG "LED" ///< Logging tag for the LED component

#include "led.h"

#define LED_GPIO_PIN 7 ///< GPIO pin number for the status LED
#define LED_SPEED_MODE LEDC_LOW_SPEED_MODE ///< LEDC speed mode (Low Speed)
#define LED_TIMER LEDC_TIMER_0             ///< LEDC timer index
#define LED_CHANNEL LEDC_CHANNEL_0         ///< LEDC channel index
#define LED_DUTY_RES LEDC_TIMER_8_BIT      ///< LEDC duty resolution (8-bit)
#define LED_FREQUENCY 5000                 ///< PWM frequency in Hz

/* --- Core Control States --- */
static uint8_t s_config_brightness =
    128; ///< User defined brightness (0-255) in RAM
static uint8_t s_active_max_brightness =
    255; ///< Active peak duty cycle for current mode (0-255)
static led_mode_t s_current_mode =
    LED_MODE_NORMAL; ///< Current operational mode
static SemaphoreHandle_t s_led_mutex =
    NULL; ///< Guard for thread-safe access to LED state
static TaskHandle_t s_generator_task_handle =
    NULL; ///< Handle for the effect generator task

/* --- Internal Generator Parameters --- */
static uint32_t s_param_period_ms = 1000; ///< Cycle duration for effects
static bool s_param_breathing =
    false; ///< True for sine-fade, False for square-blink

#ifndef M_PI
#define M_PI 3.14159265358979323846 ///< Mathematical constant PI
#endif

/**
 * @brief Updates the physical LEDC duty cycle.
 *
 * @param[in] duty 8-bit duty cycle value (0-255).
 */
static void set_pwm_duty(uint8_t duty) {
  ledc_set_duty(LED_SPEED_MODE, LED_CHANNEL, duty);
  ledc_update_duty(LED_SPEED_MODE, LED_CHANNEL);
}

/**
 * @brief Generic asynchronous generator for LED effects.
 *
 * Runs in its own task and calculates the duty cycle based on mode parameters.
 * Deletes itself when mode is set to LED_MODE_NORMAL.
 *
 * @param[in] pvParameters Unused.
 */
static void led_generator_task(void *pvParameters) {
  const uint32_t step_interval_ms = 20; // 50 Hz Refresh-Rate
  uint32_t elapsed_time_ms = 0;

  while (1) {
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);

    if (s_current_mode == LED_MODE_NORMAL) {
      s_generator_task_handle = NULL;
      xSemaphoreGive(s_led_mutex);
      vTaskDelete(NULL);
    }

    uint32_t period = s_param_period_ms;
    bool breathing = s_param_breathing;
    uint8_t max_b = s_active_max_brightness; // Use the currently valid limit

    xSemaphoreGive(s_led_mutex);

    if (breathing) {
      float target =
          (sinf(((float)elapsed_time_ms / period) * 2.0f * (float)M_PI -
                ((float)M_PI / 2.0f)) +
           1.0f) /
          2.0f;
      set_pwm_duty((uint8_t)(target * (float)max_b));
    } else {
      bool upper_half = elapsed_time_ms < (period / 2);
      set_pwm_duty(upper_half ? max_b : 0);
    }

    vTaskDelay(pdMS_TO_TICKS(step_interval_ms));
    elapsed_time_ms = (elapsed_time_ms + step_interval_ms) % period;
  }
}

/**
 * @brief Configures effect parameters and ensures the generator task is
 * running.
 *
 * @note MUST be called while holding s_led_mutex.
 *
 * @param[in] period_ms Effect cycle time.
 * @param[in] breathing True for sine-pulse, false for blink.
 */
static void update_led_generator_unsafe(uint32_t period_ms, bool breathing) {
  s_param_period_ms = period_ms;
  s_param_breathing = breathing;

  if (s_generator_task_handle == NULL) {
    BaseType_t res = xTaskCreate(led_generator_task, "led_gen", 2048, NULL, 2,
                                 &s_generator_task_handle);
    if (res != pdPASS) {
      LOGE("Failed to create LED generator task");
      s_generator_task_handle = NULL;
    }
  }
}

/* --- Public API --- */

esp_err_t led_init(void) {
  s_led_mutex = xSemaphoreCreateMutex();
  if (s_led_mutex == NULL)
    return ESP_ERR_NO_MEM;

  ledc_timer_config_t ledc_timer = {.speed_mode = LED_SPEED_MODE,
                                    .timer_num = LED_TIMER,
                                    .duty_resolution = LED_DUTY_RES,
                                    .freq_hz = LED_FREQUENCY,
                                    .clk_cfg = LEDC_AUTO_CLK};
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  ledc_channel_config_t ledc_channel = {.speed_mode = LED_SPEED_MODE,
                                        .channel = LED_CHANNEL,
                                        .timer_sel = LED_TIMER,
                                        .gpio_num = LED_GPIO_PIN,
                                        .duty = 0,
                                        .hpoint = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  set_pwm_duty(s_active_max_brightness);
  return ESP_OK;
}

void led_set_mode(led_mode_t mode) {
  if (!s_led_mutex)
    return;

  xSemaphoreTake(s_led_mutex, portMAX_DELAY);
  if (s_current_mode == mode) {
    xSemaphoreGive(s_led_mutex);
    return;
  }

  s_current_mode = mode;

  switch (mode) {
  case LED_MODE_BOOT_BREATHING:
    s_active_max_brightness = 255;
    update_led_generator_unsafe(1000, true);
    break;

  case LED_MODE_RESET:
    s_active_max_brightness = 255;
    update_led_generator_unsafe(300, true);
    break;

  case LED_MODE_ERROR:
    s_active_max_brightness = 255;
    update_led_generator_unsafe(300, false);
    break;

  case LED_MODE_WARN:
    s_active_max_brightness = s_config_brightness;
    update_led_generator_unsafe(1000, false);
    break;

  case LED_MODE_NORMAL:
    s_active_max_brightness = s_config_brightness;
    set_pwm_duty(s_active_max_brightness);
    break;

  case LED_MODE_OFF:
    s_active_max_brightness = 0;
    set_pwm_duty(s_active_max_brightness);
    break;
  }

  xSemaphoreGive(s_led_mutex);
}

void led_set_brightness(uint8_t brightness) {
  if (!s_led_mutex) {
    return;
  }

  xSemaphoreTake(s_led_mutex, portMAX_DELAY);

  s_config_brightness = brightness;

  if (s_current_mode == LED_MODE_NORMAL || s_current_mode == LED_MODE_WARN) {
    s_active_max_brightness = s_config_brightness;

    if (s_current_mode == LED_MODE_NORMAL) {
      set_pwm_duty(s_active_max_brightness);
    }
  }

  xSemaphoreGive(s_led_mutex);
}
