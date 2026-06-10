import { defineMock, MockRequest } from "vite-plugin-mock-dev-server";

type DmxPort = {
  index: number;
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
  dmx_ports: DmxPort[];
};

let data: Config = {
  connection: 0,
  ip_method: 1,
  led_brightness: 20,
  station_config: { ssid: "", password: "" },
  ap_config: { ssid: "ChaosDMX-D015", password: "ChaosDMX" },
  dmx_ports: [
    { index: 0, universe: 1, direction: 0 },
    { index: 1, universe: 2, direction: 1 },
  ] as DmxPort[],
};

function applyToDmxPortsArray(dmxPorts: DmxPort[]) {
  for (const port of dmxPorts) {
    const existingPort = data.dmx_ports.find((p) => p.index === port.index);
    if (existingPort) {
      apply(port, existingPort);
    } else {
      data.dmx_ports.push(port);
    }
  }
}

function apply(object: Record<string, any>, destination: Record<string, any>) {
  for (const key in object) {
    if (typeof object[key] === "object" && object[key] !== null) {
      if (!Array.isArray(object[key])) {
        apply(object[key], destination[key]);
      } else {
        if (key === "dmx_ports") {
          applyToDmxPortsArray(object[key]);
        } else {
          destination[key] = object[key];
        }
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
