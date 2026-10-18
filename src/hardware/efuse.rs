//! MAC address helper using ESP32-S2 eFuse.
//!
//! Reads the factory-programmed MAC address from eFuse
//! and formats it for use as `WiFi` SSID suffix, etc.

use core::fmt::Write;
use heapless::String;

/// Read the `WiFi` station MAC address from eFuse.
///
/// Returns a formatted string like `"AA:BB:CC:DD:EE:FF"`.
pub fn mac_address() -> String<17> {
    let bytes = mac_bytes();
    let mut s = String::new();
    for (i, byte) in bytes.iter().enumerate() {
        if i > 0 {
            s.push(':').ok();
        }
        push_hex_byte(&mut s, *byte);
    }
    s
}

/// Read the `WiFi` station MAC address as raw bytes.
pub fn mac_bytes() -> [u8; 6] {
    let mac = esp_hal::efuse::interface_mac_address(esp_hal::efuse::InterfaceMacAddress::Station);
    let slice = mac.as_bytes();
    let mut bytes = [0u8; 6];
    bytes.copy_from_slice(slice);
    bytes
}

/// Append a two-character uppercase hex byte to a string.
fn push_hex_byte<const N: usize>(s: &mut String<N>, byte: u8) {
    let _ = write!(s, "{byte:02X}");
}

/// Generate a default AP SSID from the MAC address.
///
/// Returns `"ChaosDMX-"` followed by the last two MAC bytes as
/// uppercase hex digits.
pub fn default_ap_ssid() -> String<32> {
    let mac = mac_bytes();
    let mut s: String<32> = String::try_from("ChaosDMX-").expect("SSID prefix fits");
    push_hex_byte(&mut s, mac[4]);
    push_hex_byte(&mut s, mac[5]);
    s
}
