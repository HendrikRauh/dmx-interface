export function enumValues<T extends object>(e: T) {
  return Object.values(e).filter((value) => typeof value === "number");
}

export enum ConnectionType {
  WIFI_AP = 0,
  WIFI_STA,
  ETHERNET,
}

export enum IpMethod {
  DHCP = 0,
  STATIC,
}

export enum ButtonAction {
  NONE,
  TOGGLE_LED,
  REBOOT,
}

export enum ButtonEvent {
  SINGLE_CLICK,
  DOUBLE_CLICK,
  MULTIPLE_CLICK,
  LONG_HOLD,
}

export function buttonActionToString(action: ButtonAction): string {
  switch (action) {
    case ButtonAction.NONE:
      return "None";
    case ButtonAction.TOGGLE_LED:
      return "Toggle LED";
    case ButtonAction.REBOOT:
      return "Reboot";
    default:
      return "Unknown";
  }
}

export function buttonEventToString(event: ButtonEvent): string {
  switch (event) {
    case ButtonEvent.SINGLE_CLICK:
      return "Single Click";
    case ButtonEvent.DOUBLE_CLICK:
      return "Double Click";
    case ButtonEvent.MULTIPLE_CLICK:
      return "Multiple Click";
    case ButtonEvent.LONG_HOLD:
      return "Long Hold";
    default:
      return "Unknown";
  }
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
