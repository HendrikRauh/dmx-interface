//! Configuration data model for the DMX interface.
//!
//! Uses `heapless` fixed-size types for `no_std` compatibility. The JSON
//! shape (field names, numeric enums, string-keyed index maps) mirrors
//! `web/src/types/config.d.ts` — the WebSocket API is the contract between
//! firmware and web UI; NVS keeps storing the same model via `postcard`.

use core::fmt;

use heapless::{FnvIndexMap, String, Vec};
use serde::{Deserialize, Serialize};

/// Maximum length for string configuration fields.
const MAX_STR_LEN: usize = 32;

/// Maximum number of DMX ports addressed by the config maps.
const MAX_DMX_PORTS: usize = 4;

/// Maximum number of button actions addressed by the config maps.
const MAX_BUTTON_ACTIONS: usize = 4;

/// Maximum size of the postcard-encoded config blob in NVS.
const MAX_CONFIG_BYTES: usize = 512;

/// Error for invalid enum discriminants arriving over the wire.
#[derive(Debug)]
pub struct InvalidValue(pub u8);

impl fmt::Display for InvalidValue {
    /// Render as `invalid enum value N`.
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "invalid enum value {}", self.0)
    }
}

/// Network connection type (web: `ConnectionType`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(try_from = "u8", into = "u8")]
pub enum ConnectionType {
    /// Access Point mode (creates its own network).
    WifiAp = 0,
    /// Station mode (connects to an existing network).
    WifiSta = 1,
    /// Wired ethernet (not implemented yet).
    Ethernet = 2,
}

impl Default for ConnectionType {
    /// Devices boot as access point until configured otherwise.
    fn default() -> Self {
        Self::WifiAp
    }
}

impl From<ConnectionType> for u8 {
    /// `WifiAp` → 0, `WifiSta` → 1, `Ethernet` → 2.
    fn from(value: ConnectionType) -> Self {
        match value {
            ConnectionType::WifiAp => 0,
            ConnectionType::WifiSta => 1,
            ConnectionType::Ethernet => 2,
        }
    }
}

impl TryFrom<u8> for ConnectionType {
    /// Discriminant conversion failed.
    type Error = InvalidValue;

    /// Map a raw discriminant back to the enum.
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(Self::WifiAp),
            1 => Ok(Self::WifiSta),
            2 => Ok(Self::Ethernet),
            other => Err(InvalidValue(other)),
        }
    }
}

/// IP assignment method (web: `IpMethod`); applied to station mode.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(try_from = "u8", into = "u8")]
pub enum IpMethod {
    /// Ask the network's DHCP server.
    #[default]
    Dhcp = 0,
    /// Use a static address (not implemented yet).
    Static = 1,
}

impl From<IpMethod> for u8 {
    /// `Dhcp` → 0, `Static` → 1.
    fn from(value: IpMethod) -> Self {
        match value {
            IpMethod::Dhcp => 0,
            IpMethod::Static => 1,
        }
    }
}

impl TryFrom<u8> for IpMethod {
    /// Discriminant conversion failed.
    type Error = InvalidValue;

    /// Map a raw discriminant back to the enum.
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(Self::Dhcp),
            1 => Ok(Self::Static),
            other => Err(InvalidValue(other)),
        }
    }
}

/// Action bound to a button event (web: `ButtonAction`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(try_from = "u8", into = "u8")]
pub enum ButtonAction {
    /// No action.
    #[default]
    None = 0,
    /// Toggle the LED effect.
    ToggleLed = 1,
    /// Reboot the device.
    Reboot = 2,
}

impl From<ButtonAction> for u8 {
    /// `None` → 0, `ToggleLed` → 1, `Reboot` → 2.
    fn from(value: ButtonAction) -> Self {
        match value {
            ButtonAction::None => 0,
            ButtonAction::ToggleLed => 1,
            ButtonAction::Reboot => 2,
        }
    }
}

impl TryFrom<u8> for ButtonAction {
    /// Discriminant conversion failed.
    type Error = InvalidValue;

    /// Map a raw discriminant back to the enum.
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(Self::None),
            1 => Ok(Self::ToggleLed),
            2 => Ok(Self::Reboot),
            other => Err(InvalidValue(other)),
        }
    }
}

/// DMX data direction (web: `direction` on `DmxPort`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(try_from = "u8", into = "u8")]
pub enum DmxDirection {
    /// Receive DMX data from a controller.
    Input = 0,
    /// Send DMX data to fixtures.
    Output = 1,
}

impl Default for DmxDirection {
    /// Output mode is the default.
    fn default() -> Self {
        Self::Output
    }
}

impl From<DmxDirection> for u8 {
    /// `Input` → 0, `Output` → 1.
    fn from(value: DmxDirection) -> Self {
        match value {
            DmxDirection::Input => 0,
            DmxDirection::Output => 1,
        }
    }
}

impl TryFrom<u8> for DmxDirection {
    /// Discriminant conversion failed.
    type Error = InvalidValue;

    /// Map a raw discriminant back to the enum.
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(Self::Input),
            1 => Ok(Self::Output),
            other => Err(InvalidValue(other)),
        }
    }
}

/// DMX port configuration (web: `DmxPort`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct DmxPortConfig {
    /// Data direction (input or output).
    pub direction: DmxDirection,
    /// DMX universe number (0–32767).
    pub universe: u16,
}

impl Default for DmxPortConfig {
    /// Output mode on universe 1.
    fn default() -> Self {
        Self {
            direction: DmxDirection::Output,
            universe: 1,
        }
    }
}

/// `WiFi` credentials for one interface (web: `WifiConfig`).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WifiConfig {
    /// Network name (empty AP SSID derives from the MAC address).
    pub ssid: String<MAX_STR_LEN>,
    /// `WiFi` password.
    pub password: String<MAX_STR_LEN>,
}

impl Default for WifiConfig {
    /// Empty credentials.
    fn default() -> Self {
        Self {
            ssid: String::new(),
            password: String::new(),
        }
    }
}

/// `serde` adapter: index maps as JSON objects with decimal string keys.
///
/// The web UI addresses ports by object key (`dmx_ports["0"]`), while
/// `serde-json-core` would emit integer keys as bare digits — invalid JSON
/// that browsers reject. Serializing through `String<N>` and parsing the
/// keys back keeps both directions valid.
mod index_map {
    use core::fmt::{self, Write as _};
    use core::marker::PhantomData;

    use heapless::{FnvIndexMap, String};
    use serde::de::{self, MapAccess, Visitor};
    use serde::ser::{SerializeMap, Serializer};
    use serde::{Deserialize, Deserializer, Serialize};

    /// Serialize `FnvIndexMap<u8, T, N>` as `{"0": …, "1": …}`.
    pub fn serialize<S: Serializer, T: Serialize, const N: usize>(
        map: &FnvIndexMap<u8, T, N>,
        serializer: S,
    ) -> Result<S::Ok, S::Error> {
        /// Wrapper struct so `serde` serializes the map key as a string.
        struct Key(u8);

        impl Serialize for Key {
            /// Write the decimal digits as a JSON string.
            fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
                let mut text = String::<4>::new();
                write!(&mut text, "{}", self.0).map_err(serde::ser::Error::custom)?;
                serializer.serialize_str(text.as_str())
            }
        }

        let mut out = serializer.serialize_map(Some(map.len()))?;
        for (key, value) in map {
            out.serialize_entry(&Key(*key), value)?;
        }
        out.end()
    }

    /// Deserialize `{"0": …, "1": …}` back into `FnvIndexMap<u8, T, N>`.
    pub fn deserialize<'de, D: Deserializer<'de>, T: Deserialize<'de>, const N: usize>(
        deserializer: D,
    ) -> Result<FnvIndexMap<u8, T, N>, D::Error> {
        /// Visitor collecting string-keyed entries into the map.
        struct MapVisitor<T, const N: usize>(PhantomData<T>);

        impl<'de, T: Deserialize<'de>, const N: usize> Visitor<'de> for MapVisitor<T, N> {
            /// The map produced by this visitor.
            type Value = FnvIndexMap<u8, T, N>;

            /// Human-readable description for parse errors.
            fn expecting(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.write_str("a map with decimal string keys")
            }

            /// Parse each `"key": value` entry into the map.
            fn visit_map<A: MapAccess<'de>>(self, mut access: A) -> Result<Self::Value, A::Error> {
                let mut map = FnvIndexMap::new();
                while let Some((key, value)) = access.next_entry::<String<8>, T>()? {
                    let index: u8 = key.parse().map_err(de::Error::custom)?;
                    map.insert(index, value)
                        .map_err(|_| de::Error::custom("config map full"))?;
                }
                Ok(map)
            }
        }

        deserializer.deserialize_map(MapVisitor(PhantomData))
    }
}

/// Complete device configuration (web: `Config`).
///
/// Field suffixes mirror the web JSON contract — renaming for the lint
/// would break the API.
#[allow(clippy::struct_field_names)]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    /// Configuration schema version (for future migration).
    pub version: u8,
    /// Network connection type.
    pub connection: ConnectionType,
    /// IP assignment method for station mode.
    pub ip_method: IpMethod,
    /// LED brightness (0–255).
    pub led_brightness: u8,
    /// `WiFi` station credentials.
    pub station_config: WifiConfig,
    /// `WiFi` access-point credentials (empty SSID derives from the MAC).
    pub ap_config: WifiConfig,
    /// DMX ports by port index.
    #[serde(with = "index_map")]
    pub dmx_ports: FnvIndexMap<u8, DmxPortConfig, MAX_DMX_PORTS>,
    /// Button actions by button index.
    #[serde(with = "index_map")]
    pub button_actions: FnvIndexMap<u8, ButtonAction, MAX_BUTTON_ACTIONS>,
}

impl Default for Config {
    /// Factory defaults: AP mode, DMX0 output on universe 1, DMX1 input on
    /// universe 2, dim LED.
    fn default() -> Self {
        let ap_config = WifiConfig {
            password: String::from("ChaosDMX"),
            ..WifiConfig::default()
        };
        let mut dmx_ports = FnvIndexMap::new();
        let _ = dmx_ports.insert(0, DmxPortConfig::default());
        let _ = dmx_ports.insert(
            1,
            DmxPortConfig {
                direction: DmxDirection::Input,
                universe: 2,
            },
        );
        Self {
            version: 1,
            connection: ConnectionType::default(),
            ip_method: IpMethod::default(),
            led_brightness: 25,
            station_config: WifiConfig::default(),
            ap_config,
            dmx_ports,
            button_actions: FnvIndexMap::new(),
        }
    }
}

impl Config {
    /// Serialize config to bytes using postcard (binary format).
    ///
    /// Returns a `Vec` with max [`MAX_CONFIG_BYTES`] capacity.
    pub fn to_bytes(&self) -> Result<Vec<u8, MAX_CONFIG_BYTES>, postcard::Error> {
        postcard::to_vec(self)
    }

    /// Deserialize config from bytes.
    pub fn from_bytes(data: &[u8]) -> Result<Self, postcard::Error> {
        postcard::from_bytes(data)
    }

    /// Pushes this configuration into every driver that consumes it.
    ///
    /// Called once at boot after loading and again whenever the config
    /// changes (WebSocket `config.set`, factory reset) — the single place
    /// mapping config fields onto driver state. Network changes apply on
    /// the next reboot.
    pub fn apply(&self) {
        crate::hardware::led::set_brightness(self.led_brightness);
    }

    /// Merge a partial patch onto this config (present fields win).
    ///
    /// This is the server-side counterpart of the web UI's
    /// `DeepPartial<Config>` PATCH semantics: nested objects
    /// (`ap_config`, `station_config`, `dmx_ports[*]`) merge field by
    /// field, map entries merge per key, scalar fields replace when
    /// present.
    ///
    /// Returns `false` when the patch is rejected — a map key outside
    /// `0..MAX_*` or a map that is already full. Keys are validated before
    /// anything is merged, so a rejected patch leaves the config untouched
    /// (HTTP/WS answer 400 and must not persist).
    pub fn merge_patch(&mut self, patch: ConfigPatch) -> bool {
        if patch
            .dmx_ports
            .keys()
            .any(|&key| usize::from(key) >= MAX_DMX_PORTS)
            || patch
                .button_actions
                .keys()
                .any(|&key| usize::from(key) >= MAX_BUTTON_ACTIONS)
        {
            return false;
        }
        if let Some(version) = patch.version {
            self.version = version;
        }
        if let Some(connection) = patch.connection {
            self.connection = connection;
        }
        if let Some(ip_method) = patch.ip_method {
            self.ip_method = ip_method;
        }
        if let Some(led_brightness) = patch.led_brightness {
            self.led_brightness = led_brightness;
        }
        if let Some(station_config) = patch.station_config {
            station_config.apply(&mut self.station_config);
        }
        if let Some(ap_config) = patch.ap_config {
            ap_config.apply(&mut self.ap_config);
        }
        for (key, port_patch) in patch.dmx_ports {
            let mut port = self.dmx_ports.get(&key).copied().unwrap_or_default();
            port_patch.apply(&mut port);
            if self.dmx_ports.insert(key, port).is_err() {
                return false;
            }
        }
        for (key, action) in patch.button_actions {
            if self.button_actions.insert(key, action).is_err() {
                return false;
            }
        }
        true
    }
}

/// One [`WifiConfig`] entry as it arrives in a patch (web: `DeepPartial`).
///
/// The UI edits credentials leaf by leaf, so both fields are optional
/// and absent fields keep their current value.
#[derive(Debug, Default, Deserialize)]
pub struct WifiConfigPatch {
    /// Optional network name.
    pub ssid: Option<String<MAX_STR_LEN>>,
    /// Optional password.
    pub password: Option<String<MAX_STR_LEN>>,
}

impl WifiConfigPatch {
    /// Move the present fields onto `target`; absent fields stay untouched.
    fn apply(self, target: &mut WifiConfig) {
        if let Some(ssid) = self.ssid {
            target.ssid = ssid;
        }
        if let Some(password) = self.password {
            target.password = password;
        }
    }
}

/// One [`DmxPortConfig`] entry as it arrives in a patch (web: `DeepPartial`).
#[derive(Debug, Default, Deserialize)]
pub struct DmxPortPatch {
    /// Optional data direction.
    pub direction: Option<DmxDirection>,
    /// Optional DMX universe number.
    pub universe: Option<u16>,
}

impl DmxPortPatch {
    /// Move the present fields onto `target`; absent fields stay untouched.
    fn apply(self, target: &mut DmxPortConfig) {
        if let Some(direction) = self.direction {
            target.direction = direction;
        }
        if let Some(universe) = self.universe {
            target.universe = universe;
        }
    }
}

/// Partial configuration (web: `DeepPartial<Config>`).
///
/// Every field is optional — including nested objects: credentials and
/// DMX-port entries arrive leaf by leaf (the UI only sends changed
/// leaves) and merge field-wise onto the stored values.
#[derive(Debug, Default, Deserialize)]
pub struct ConfigPatch {
    /// Optional schema version.
    pub version: Option<u8>,
    /// Optional connection type.
    pub connection: Option<ConnectionType>,
    /// Optional IP method.
    pub ip_method: Option<IpMethod>,
    /// Optional LED brightness.
    pub led_brightness: Option<u8>,
    /// Optional station credentials (fields merge individually).
    pub station_config: Option<WifiConfigPatch>,
    /// Optional access-point credentials (fields merge individually).
    pub ap_config: Option<WifiConfigPatch>,
    /// Optional per-port overrides (merged per key and field).
    #[serde(default, with = "index_map")]
    pub dmx_ports: FnvIndexMap<u8, DmxPortPatch, MAX_DMX_PORTS>,
    /// Optional per-button overrides (merged per key).
    #[serde(default, with = "index_map")]
    pub button_actions: FnvIndexMap<u8, ButtonAction, MAX_BUTTON_ACTIONS>,
}

/// Serialization round-trip tests.
#[cfg(test)]
mod tests {
    use super::*;

    /// Encode → decode must preserve the important fields.
    #[test]
    fn config_roundtrip() {
        let config = Config::default();
        let bytes = config.to_bytes().unwrap();
        let restored = Config::from_bytes(&bytes).unwrap();
        assert_eq!(config.connection, restored.connection);
        assert_eq!(config.led_brightness, restored.led_brightness);
        assert_eq!(
            config.dmx_ports.get(&0).map(|port| port.universe),
            restored.dmx_ports.get(&0).map(|port| port.universe)
        );
    }
}
