//! Hardware abstraction for status LED effects.
//!
//! Provides PWM-based LED control via ESP-IDF LEDC peripheral,
//! supporting various blinking and breathing patterns.

use esp_hal::ledc::{
    LowSpeed,
    channel::{self, ChannelIFace},
};

/// Available LED effects for the status LED.
///
/// Each effect modulates the LED brightness over time using PWM duty cycle.
pub enum LedEffect {
    /// LED is off (0% duty cycle).
    Off,
    /// LED is on at a constant brightness.
    Solid,
    /// LED fades in and out with a triangular waveform (2s period).
    Breathing,
    /// LED toggles on/off every 500ms.
    Blinking,
    /// LED toggles on/off every 100ms.
    FastBlink,
}

/// Scale an 8-bit brightness (0–255) to a duty cycle in percent (0–100).
///
/// The intermediate math runs in `u16`, which cannot overflow here
/// (`255 * 100 = 25_500 < u16::MAX`); `saturating_mul` makes that bound
/// explicit for `clippy::arithmetic_side_effects`.
fn brightness_pct(brightness: u8) -> u8 {
    let pct = u16::from(brightness).saturating_mul(100) / 255;
    u8::try_from(pct).unwrap_or(100)
}

/// Apply the given LED effect to a LEDC channel.
///
/// # Arguments
/// * `channel` - The LEDC PWM channel to control.
/// * `effect` - The desired LED effect.
/// * `brightness` - Maximum brightness level (0–255, scaled to 0–100% duty).
/// * `elapsed_ms` - Milliseconds since the effect was started (for timing).
pub fn apply(
    channel: &channel::Channel<'_, LowSpeed>,
    effect: &LedEffect,
    brightness: u8,
    elapsed_ms: u64,
) {
    match effect {
        LedEffect::Off => {
            channel.set_duty(0).ok();
        }
        LedEffect::Solid => {
            channel.set_duty(brightness_pct(brightness)).ok();
        }
        LedEffect::Breathing => {
            // 2 s triangle ramp: 0 → 1000 → 0, then scaled by the brightness.
            let pos = elapsed_ms % 2000;
            let ramp = if pos < 1000 {
                pos
            } else {
                2000u64.saturating_sub(pos)
            };
            let duty = ramp.saturating_mul(u64::from(brightness_pct(brightness))) / 1000;
            channel.set_duty(u8::try_from(duty).unwrap_or(100)).ok();
        }
        LedEffect::Blinking => {
            let on = (elapsed_ms % 1000) < 500;
            channel
                .set_duty(if on { brightness_pct(brightness) } else { 0 })
                .ok();
        }
        LedEffect::FastBlink => {
            let on = (elapsed_ms % 200) < 100;
            channel
                .set_duty(if on { brightness_pct(brightness) } else { 0 })
                .ok();
        }
    }
}
