//! NVS persistent storage for device configuration.
//!
//! Wraps `esp-nvs` to provide config load/save/clear operations.
//! Config is serialized as a binary blob via `postcard`.

extern crate alloc;

use alloc::vec::Vec;
use esp_hal::peripherals::FLASH;
use esp_nvs::{Key, Nvs};
use esp_println::println;
use esp_storage::FlashStorage;

use crate::config::Config;

/// NVS partition offset (default for ESP32-S2).
const NVS_OFFSET: usize = 0x9000;

/// NVS partition size (default for ESP32-S2: 24 KiB).
const NVS_SIZE: usize = 0x6000;

/// NVS namespace for config storage.
const NS_CONFIG: Key = Key::from_str("config");

/// NVS key for the config blob.
const KEY_CFG: Key = Key::from_str("cfg");

/// Persistent storage handle wrapping esp-nvs.
pub struct ConfigStorage<'a> {
    nvs: Nvs<FlashStorage<'a>>,
}

impl ConfigStorage<'_> {
    /// Initialize NVS storage.
    ///
    /// Must be called once at startup before any load/save operations.
    /// Takes ownership of the FLASH peripheral (not used elsewhere).
    pub fn new(flash: FLASH<'static>) -> ConfigStorage<'static> {
        let flash = FlashStorage::new(flash);
        let nvs = Nvs::new(NVS_OFFSET, NVS_SIZE, flash).expect("NVS init failed");
        ConfigStorage { nvs }
    }

    /// Load config from NVS.
    ///
    /// Returns `Config::default()` if no config is stored or if deserialization fails.
    pub fn load(&mut self) -> Config {
        match self.nvs.get::<Vec<u8>>(&NS_CONFIG, &KEY_CFG) {
            Ok(bytes) => match Config::from_bytes(&bytes) {
                Ok(config) => {
                    println!("Config loaded from NVS ({} bytes)", bytes.len());
                    config
                }
                Err(_e) => {
                    println!("Config deserialization failed: {_e:?}, using defaults");
                    Config::default()
                }
            },
            Err(_e) => {
                println!("NVS read failed: {_e:?}, using defaults");
                Config::default()
            }
        }
    }

    /// Save config to NVS.
    ///
    /// Serializes the config to bytes and writes it as a blob.
    pub fn save(&mut self, config: &Config) {
        match config.to_bytes() {
            Ok(bytes) => match self.nvs.set(&NS_CONFIG, &KEY_CFG, bytes.as_slice()) {
                Ok(()) => println!("Config saved to NVS ({} bytes)", bytes.len()),
                Err(_e) => println!("NVS write failed: {_e:?}"),
            },
            Err(_e) => println!("Config serialization failed: {_e:?}"),
        }
    }

    /// Clear all config data from NVS (reverts to defaults on next load).
    pub fn clear(&mut self) {
        match self.nvs.delete(&NS_CONFIG, &KEY_CFG) {
            Ok(()) => println!("Config cleared from NVS"),
            Err(_e) => println!("NVS clear failed: {_e:?}"),
        }
    }
}
