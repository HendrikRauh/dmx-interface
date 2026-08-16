import { defineMock, MockRequest } from "vite-plugin-mock-dev-server";

import { deepMerged } from "../src/util/deep-merge";

let data: Config = {
  connection: 0,
  ip_method: 1,
  led_brightness: 20,
  station_config: { ssid: "", password: "" },
  ap_config: { ssid: "ChaosDMX-D015", password: "ChaosDMX" },
  dmx_ports: {
    0: { universe: 1, direction: 0 },
    1: { universe: 2, direction: 1 },
  } as { [index: number]: DmxPort },
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
