#define LOG_TAG "DMX" ///< "DMX" log tag for this file

#include <stdint.h>

#include "dmx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"

static void dmx_sender_task(void *pv_parameters) {
  dmx_port_t port = (dmx_port_t)(intptr_t)pv_parameters;
  TickType_t last_wake_time = xTaskGetTickCount();

  while (1) {
    send_dmx(port);
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
  }
}

void init_dmx(dmx_port_t port, int pin_tx, int pin_rx) {
  LOGI("Initializing DMX port %d on pin %d (TX) and %d (RX)", port, pin_tx,
       pin_rx);

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {};
  dmx_driver_install(port, &config, personalities, 0);

  dmx_set_pin(port, pin_tx, pin_rx, DMX_PIN_NO_CHANGE); // RTS pin is not used

  BaseType_t task_created = xTaskCreate(dmx_sender_task, "dmx_sender", 2048,
                                        (void *)(intptr_t)port, 5, NULL);
  if (task_created != pdPASS) {
    LOGE("Failed to create DMX sender task for port %d", port);
  }
}

void send_dmx(dmx_port_t port) {
  LOGD("Sending DMX data on port %d", port);

  uint8_t dmx_data[DMX_PACKET_SIZE] = {DMX_SC, 0, 0, 0, 255, 100, 0, 100, 0};
  dmx_write(port, dmx_data, DMX_PACKET_SIZE);
  dmx_send(port);
}
