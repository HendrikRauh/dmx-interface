import { defineMock, MockRequest } from "vite-plugin-mock-dev-server";

type DmxPort = {
  universe: number;
  direction: number;
};

type WifiConfig = {
  ssid: string;
  password: string;
};

type Config = {
  connection: number;
  ip_method: number;
  led_brightness: number;
  station_config: WifiConfig;
  ap_config: WifiConfig;
  dmx_ports: {
    [index: number]: DmxPort;
  };
};

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

function apply(object: Record<string, any>, destination: Record<string, any>) {
  for (const key in object) {
    if (typeof object[key] === "object" && object[key] !== null) {
      if (!Array.isArray(object[key])) {
        apply(object[key], destination[key]);
      } else {
        destination[key] = object[key];
      }
    } else {
      destination[key] = object[key];
    }
  }
}

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
      apply(body, data);
    },
  },
]);
