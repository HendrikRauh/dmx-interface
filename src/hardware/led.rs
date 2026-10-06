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

/// Apply the given LED effect to a LEDC channel.
///
/// # Arguments
/// * `channel` - The LEDC PWM channel to control.
/// * `effect` - The desired LED effect.
/// * `brightness` - Maximum brightness level (0–255, scaled to 0–100% duty).
/// * `elapsed_ms` - Milliseconds since the effect was started (for timing).
pub fn apply(
    channel: &mut channel::Channel<'_, LowSpeed>,
    effect: &LedEffect,
    brightness: u8,
    elapsed_ms: u64,
) {
    match effect {
        LedEffect::Off => {
            channel.set_duty(0).ok();
        }
        LedEffect::Solid => {
            channel.set_duty(brightness * 100 / 255).ok();
        }
        LedEffect::Breathing => {
            let period = 2000u64;
            let pos = elapsed_ms % period;
            let t = if pos < period / 2 {
                pos as f32 / (period as f32 / 2.0)
            } else {
                2.0 - pos as f32 / (period as f32 / 2.0)
            };
            let duty_pct = (t * brightness as f32 / 255.0 * 100.0) as u8;
            channel.set_duty(duty_pct.min(100)).ok();
        }
        LedEffect::Blinking => {
            let on = (elapsed_ms % 1000) < 500;
            channel
                .set_duty(if on { brightness * 100 / 255 } else { 0 })
                .ok();
        }
        LedEffect::FastBlink => {
            let on = (elapsed_ms % 200) < 100;
            channel
                .set_duty(if on { brightness * 100 / 255 } else { 0 })
                .ok();
        }
    }
}
