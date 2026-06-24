export function enumValues<T extends object>(e: T) {
  return Object.values(e).filter((value) => typeof value === "number");
}

export enum ConnectionType {
  WIFI_AP = 0,
  WIFI_STA,
  ETHERNET,
}

export function connectionTypeToString(type: ConnectionType): string {
  switch (type) {
    case ConnectionType.WIFI_AP:
      return "WiFi Access Point";
    case ConnectionType.WIFI_STA:
      return "WiFi Station";
    case ConnectionType.ETHERNET:
      return "Ethernet";
    default:
      return "Unknown";
  }
}
