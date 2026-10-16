import { defineMock, MockRequest } from "vite-plugin-mock-dev-server";

import { deepMerged } from "../src/util/deep-merge";

// Mirrors `Config::default()` in src/config.rs — keep in sync when factory
// defaults change (version 1, AP mode/DHCP, DMX0 out/U1 + DMX1 in/U2,
// dim LED, single click toggles the LED, triple click reboots).
let data: Config = {
  version: 1,
  connection: 0,
  ip_method: 0,
  led_brightness: 25,
  station_config: { ssid: "", password: "" },
  ap_config: { ssid: "ChaosDMX-D015", password: "ChaosDMX" },
  dmx_ports: {
    0: { universe: 1, direction: 1 },
    1: { universe: 2, direction: 0 },
  } as { [index: number]: DmxPort },
  button_actions: {
    0: 1,
    1: 0,
    2: 2,
    3: 0,
  } as { [index: number]: ButtonAction },
};

export default defineMock([
  {
    url: "/api/config",
    method: "GET",
    body: () => data,
  },
  {
    url: "/api/config",
    method: "POST",
    body: ({ body }: MockRequest) => {
      data = deepMerged(data, body);
    },
  },
]);
