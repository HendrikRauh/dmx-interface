/**
 * @file button_actions.c
 * @brief Dynamic hardware button execution mapped directly to app
 * configuration.
 */

/**
 * @brief Logging tag for the button actions component.
 */
#define LOG_TAG "BUTTON"

#include "button_actions.h"
#include "button_gpio.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "iot_button.h"
#include "led.h"
#include "logger.h"

/**
 * @brief GPIO number assigned to the hardware button.
 */
#define BUTTON_GPIO_NUM GPIO_NUM_5

/**
 * @brief Duration (ms) the button must be held for a reset trigger.
 */
#define RESET_HOLD_TIME_MS 3000

/**
 * @brief Internal handle for the IoT button instance.
 */
static button_handle_t s_btn_handle = NULL;

/**
 * @brief Executes the action compiled into the configuration for the triggered
 * event.
 *
 * This evaluates the current NVS/RAM configuration dynamically on every click,
 * making it fully hot-swappable from the Web UI without registering callbacks.
 *
 * @param assigned_action The configured action to execute.
 */
static void execute_button_action(config_button_action_t assigned_action) {
  switch (assigned_action) {
  case APP_BUTTON_ACTION_TOGGLE_LED: {
    LOGI("Button action: Toggle LED Brightness");

    // TODO: Extract in component/led

    static const uint8_t levels[] = {0, 1, 20, 100, 255};
    const uint8_t num_levels = sizeof(levels) / sizeof(levels[0]);

    uint8_t current_brightness = config_get_led_brightness();
    uint8_t next_brightness = levels[0];

    for (uint8_t i = 0; i < num_levels; i++) {
      if (current_brightness < levels[i]) {
        next_brightness = levels[i];
        break;
      }
    }

    config_set_led_brightness(next_brightness);
    config_save();
    break;
  }

  case APP_BUTTON_ACTION_REBOOT:
    LOGI("Button action: Reboot");
    fflush(stdout);
    esp_restart();
    break;

  case APP_BUTTON_ACTION_NONE:
  default:
    LOGI("No action or APP_BUTTON_ACTION_NONE assigned to this event.");
    break;
  }
}

/* --- Internal Driver Event Callbacks --- */

/**
 * @brief Callback for single click events.
 * @param arg Pointer to the button handle (unused).
 * @param usr_data User data passed during registration (unused).
 */
static void iot_button_single_click_cb(void *arg, void *usr_data) {
  LOGI("Single click detected.");
  execute_button_action(
      config_get_button_action(APP_BUTTON_EVENT_SINGLE_CLICK));
}

/**
 * @brief Callback for double click events.
 * @param arg Pointer to the button handle (unused).
 * @param usr_data User data passed during registration (unused).
 */
static void iot_button_double_click_cb(void *arg, void *usr_data) {
  LOGI("Double click detected.");
  execute_button_action(
      config_get_button_action(APP_BUTTON_EVENT_DOUBLE_CLICK));
}

/**
 * @brief Callback for multiple click events.
 * @param arg Pointer to the button handle (unused).
 * @param usr_data User data passed during registration (unused).
 */
static void iot_button_multiple_click_cb(void *arg, void *usr_data) {
  LOGI("Multiple click detected.");
  execute_button_action(
      config_get_button_action(APP_BUTTON_EVENT_MULTIPLE_CLICK));
}

/* --- Public API --- */

esp_err_t button_init(void) {
  LOGI("Initializing runtime-configurable hardware button handler...");

  // 1. General button configuration (Logic)
  button_config_t btn_cfg = {
      .long_press_time = RESET_HOLD_TIME_MS,
  };

  // 2. Hardware-specific configuration (GPIO driver)
  button_gpio_config_t gpio_cfg = {
      .gpio_num = BUTTON_GPIO_NUM,
      .active_level = 0, /* Active Low */
  };

  // 3. Create GPIO button device
  esp_err_t err =
      iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &s_btn_handle);
  if (err != ESP_OK || s_btn_handle == NULL) {
    LOGE("Failed to instantiate IoT button driver!");
    return ESP_FAIL;
  }

  // 4. Register event callbacks
  // Note: MULTIPLE_CLICK requires arguments to define the click count.
  // Using static to ensure the driver has a stable pointer to the
  // configuration.
  static button_event_args_t multi_args = {.multiple_clicks = {.clicks = 3}};

  ESP_ERROR_CHECK(iot_button_register_cb(s_btn_handle, BUTTON_SINGLE_CLICK,
                                         NULL, iot_button_single_click_cb,
                                         NULL));
  ESP_ERROR_CHECK(iot_button_register_cb(s_btn_handle, BUTTON_DOUBLE_CLICK,
                                         NULL, iot_button_double_click_cb,
                                         NULL));

  ESP_ERROR_CHECK(iot_button_register_cb(s_btn_handle, BUTTON_MULTIPLE_CLICK,
                                         &multi_args,
                                         iot_button_multiple_click_cb, NULL));

  LOGI("Dynamic button router successfully running.");
  return ESP_OK;
}

bool button_is_pressed(void) {
  if (s_btn_handle == NULL) {
    return false;
  }
  uint8_t level = iot_button_get_key_level(s_btn_handle);
  return level == 1;
}

void button_disable_factory_reset(void) {
  // No action needed as long-press is not registered in button_init
  // and handled manually in app_main during the boot phase.
}
