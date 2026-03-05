#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

  esp_err_t wifi_start_ap(const char *ssid, const char *password, uint8_t channel, uint8_t max_connections);
  void wifi_stop_ap(void);

#ifdef __cplusplus
}
#endif
