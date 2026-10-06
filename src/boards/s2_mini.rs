//! Lolin S2 Mini board configuration.
//!
//! Pin definitions and hardware constants for the Lolin S2 Mini (ESP32-S2).
//! This is the primary target board.

#![allow(dead_code)]

/// Human-readable board identifier.
pub const BOARD_NAME: &str = "Lolin S2 Mini";

// ─────────────────────────────────────────────────────────────────────────────
// DMX / RS-485
// ─────────────────────────────────────────────────────────────────────────────

/// UART TX pin for DMX/RS-485 output.
pub const DMX_UART_TX: u8 = 17;

/// UART RX pin for DMX/RS-485 input.
pub const DMX_UART_RX: u8 = 18;

/// GPIO pin for RS-485 DE/RE (direction enable/disable).
pub const DMX_UART_DE_RE: u8 = 16;

// ─────────────────────────────────────────────────────────────────────────────
// STATUS LED (PWM)
// ─────────────────────────────────────────────────────────────────────────────

/// GPIO pin for the external status LED (via MOSFET, if connected).
pub const LED_GPIO: u8 = 7;

/// GPIO pin for the on-board status LED (active-low).
pub const ONBOARD_LED_GPIO: u8 = 15;

// ─────────────────────────────────────────────────────────────────────────────
// USER BUTTON (external, pull-up)
// ─────────────────────────────────────────────────────────────────────────────

/// GPIO pin for the external user button (active low, pull-up).
pub const BUTTON_GPIO: u8 = 5;

// ─────────────────────────────────────────────────────────────────────────────
// BOOT BUTTON (on-board)
// ─────────────────────────────────────────────────────────────────────────────

/// GPIO pin for the on-board BOOT button.
pub const BOOT_BUTTON_GPIO: u8 = 0;

// ─────────────────────────────────────────────────────────────────────────────
// CONSOLE UART (USB CDC / USB Serial JTAG)
// ─────────────────────────────────────────────────────────────────────────────

/// UART TX pin for console output (USB CDC).
pub const CONSOLE_TX: u8 = 43;

/// UART RX pin for console input (USB CDC).
pub const CONSOLE_RX: u8 = 44;

// ─────────────────────────────────────────────────────────────────────────────
// USB-OTG (native USB-CDC)
// ─────────────────────────────────────────────────────────────────────────────

/// USB D+ pin (USB-OTG, fixed by hardware).
pub const USB_DP_GPIO: u8 = 20;

/// USB D- pin (USB-OTG, fixed by hardware).
pub const USB_DM_GPIO: u8 = 19;

// ─────────────────────────────────────────────────────────────────────────────
// SPI (W5500 Ethernet — future)
// ─────────────────────────────────────────────────────────────────────────────

/// SPI chip select pin for W5500 Ethernet module.
pub const W5500_CS: u8 = 34;

/// SPI MOSI pin for W5500 Ethernet module.
pub const W5500_MOSI: u8 = 35;

/// SPI SCK pin for W5500 Ethernet module.
pub const W5500_SCK: u8 = 36;

/// SPI MISO pin for W5500 Ethernet module.
pub const W5500_MISO: u8 = 37;
