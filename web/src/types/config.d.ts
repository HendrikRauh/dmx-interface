type ButtonAction = 0 | 1 | 2;

type DmxPort = {
  universe: number;
  direction: number;
};

type WifiConfig = {
  ssid: string;
  password: string;
};

type Config = {
  version: number;
  connection: ConnectionType;
  ip_method: IpMethod;
  led_brightness: number;
  station_config: WifiConfig;
  ap_config: WifiConfig;
  dmx_ports: {
    [index: number]: DmxPort;
  };
  button_actions: {
    [index: number]: ButtonAction;
  };
};
