//! Configuration data model for the DMX interface.
//!
//! Uses `heapless` fixed-size types for no_std compatibility.
//! Config is serialized with `postcard` (binary) for NVS storage.

use heapless::{String, Vec};
use serde::{Deserialize, Serialize};

/// Maximum length for string configuration fields.
const MAX_STR_LEN: usize = 32;

/// DMX port mode.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DmxMode {
    /// DMX output mode (sends DMX data to fixtures).
    Output,
    /// DMX input mode (receives DMX data from controller).
    Input,
}

impl Default for DmxMode {
    /// Output mode is the default.
    fn default() -> Self {
        Self::Output
    }
}

/// `WiFi` operating mode.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum WifiMode {
    /// Access Point mode (creates its own network).
    Ap,
    /// Station mode (connects to existing network).
    Sta,
}

impl Default for WifiMode {
    /// Devices boot as access point until configured otherwise.
    fn default() -> Self {
        Self::Ap
    }
}

/// DMX port configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DmxPortConfig {
    /// Port mode (input or output).
    pub mode: DmxMode,
    /// DMX universe number (0–512).
    pub universe: u16,
}

impl Default for DmxPortConfig {
    /// Output mode on universe 1.
    fn default() -> Self {
        Self {
            mode: DmxMode::Output,
            universe: 1,
        }
    }
}

/// `WiFi` station configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WifiStaConfig {
    /// SSID to connect to.
    pub ssid: String<MAX_STR_LEN>,
    /// `WiFi` password.
    pub password: String<MAX_STR_LEN>,
}

impl Default for WifiStaConfig {
    /// Empty credentials — station mode is not configured yet.
    fn default() -> Self {
        Self {
            ssid: String::new(),
            password: String::new(),
        }
    }
}

/// `WiFi` access point configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WifiApConfig {
    /// AP SSID (default: derived from MAC address).
    pub ssid: String<MAX_STR_LEN>,
    /// AP password.
    pub password: String<MAX_STR_LEN>,
}

impl Default for WifiApConfig {
    /// Empty SSID (derived from MAC at runtime), password `ChaosDMX`.
    fn default() -> Self {
        Self {
            ssid: String::new(),
            password: String::from("ChaosDMX"),
        }
    }
}

/// Complete device configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    /// `WiFi` mode (AP or STA).
    pub wifi_mode: WifiMode,
    /// `WiFi` AP configuration.
    pub wifi_ap: WifiApConfig,
    /// `WiFi` STA configuration.
    pub wifi_sta: WifiStaConfig,
    /// DMX port 0 configuration.
    pub dmx0: DmxPortConfig,
    /// DMX port 1 configuration.
    pub dmx1: DmxPortConfig,
    /// LED brightness (0–255).
    pub led_brightness: u8,
    /// Configuration version (for future migration).
    pub version: u8,
}

impl Default for Config {
    /// Factory defaults: AP mode, DMX0 output / DMX1 input, dim LED.
    fn default() -> Self {
        Self {
            wifi_mode: WifiMode::default(),
            wifi_ap: WifiApConfig::default(),
            wifi_sta: WifiStaConfig::default(),
            dmx0: DmxPortConfig::default(),
            dmx1: DmxPortConfig {
                mode: DmxMode::Input,
                universe: 2,
            },
            led_brightness: 25, // ~10%
            version: 0,
        }
    }
}

impl Config {
    /// Serialize config to bytes using postcard (binary format).
    ///
    /// Returns a `Vec` with max 256 bytes capacity.
    pub fn to_bytes(&self) -> Result<Vec<u8, 256>, postcard::Error> {
        postcard::to_vec(self)
    }

    /// Deserialize config from bytes.
    pub fn from_bytes(data: &[u8]) -> Result<Self, postcard::Error> {
        postcard::from_bytes(data)
    }
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
        assert_eq!(config.wifi_mode, restored.wifi_mode);
        assert_eq!(config.led_brightness, restored.led_brightness);
        assert_eq!(config.dmx0.universe, restored.dmx0.universe);
    }
}
