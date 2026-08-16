type DmxPort = {
  universe: number;
  direction: number;
};

type WifiConfig = {
  ssid: string;
  password: string;
};

type Config = {
  connection: ConnectionType;
  ip_method: number;
  led_brightness: number;
  station_config: WifiConfig;
  ap_config: WifiConfig;
  dmx_ports: {
    [index: number]: DmxPort;
  };
};
