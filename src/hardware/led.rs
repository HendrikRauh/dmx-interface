//! Status LED: LEDC PWM patterns driven by a dedicated embassy task.
//!
//! The LED is the device status indicator. Patterns and timings follow
//! the mockups in `assets/led/*.svg`:
//!
//! | [`LedStatus`] | Pattern                    | Period |
//! | ------------- | -------------------------- | ------ |
//! | `Off`         | dark                       | —      |
//! | `Startup`     | breathing, peak brightness | 1000ms |
//! | `Resetting`   | fast breathing, peak       | 300ms  |
//! | `Ok`          | solid, config brightness   | —      |
//! | `Warning`     | blink, config brightness   | 1000ms |
//! | `Error`       | blink, peak brightness     | 200ms  |
//!
//! All timing lives inside [`task`]; every other task (or interrupt) only
//! calls [`set`] — a lock-free atomic store. Config-driven brightness flows
//! in through [`set_brightness`], pushed by
//! [`crate::config::Config::apply`].

use core::{
    f32::consts::{FRAC_PI_2, TAU},
    sync::atomic::{AtomicU8, Ordering},
};

use embassy_time::{Duration, Instant, Timer};
use esp_hal::{
    ledc::{
        LSGlobalClkSource, Ledc, LowSpeed,
        channel::{self, ChannelHW, ChannelIFace},
        timer::{self, LSClockSource, TimerIFace},
    },
    peripherals::{GPIO7, LEDC},
    time::Rate,
};

/// Breathing period of [`LedStatus::Startup`] (1000 ms per `assets/led/boot.svg`).
const STARTUP_PERIOD_MS: u64 = 1000;
/// Breathing period of [`LedStatus::Resetting`] (300 ms per `assets/led/reset.svg`).
const RESETTING_PERIOD_MS: u64 = 300;
/// Blink period of [`LedStatus::Warning`] (1000 ms per `assets/led/slow.svg`).
const WARNING_PERIOD_MS: u64 = 1000;
/// Blink period of [`LedStatus::Error`] (200 ms per `assets/led/fast.svg`).
const ERROR_PERIOD_MS: u64 = 200;

/// Effect refresh interval.
const TICK: Duration = Duration::from_millis(10);

/// Brightness of the attention states (full scale).
const PEAK_BRIGHTNESS: u8 = 255;

/// Maximum duty count for the configured 14-bit resolution
/// ([`timer::config::Duty::Duty14Bit`], i.e. 2^14 − 1).
///
/// Duty is driven via [`ChannelHW::set_duty_hw`] (raw counts) instead of the
/// percent-based [`ChannelIFace::set_duty`]: the percent API quantizes every
/// update to whole percent, which makes the breathing curve visibly step at
/// low brightness. Raw counts keep all 16384 hardware levels.
const DUTY_MAX: u16 = 0x3FFF;

/// Brightness assumed until [`set_brightness`] delivers the configured
/// value (mirrors `Config::default().led_brightness`).
const DEFAULT_BRIGHTNESS: u8 = 25;

/// Semantic status shown by the LED (timings in the module docs).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LedStatus {
    /// LED off (duty 0).
    Off,
    /// Slow breathing while starting up (1000 ms, peak brightness).
    Startup,
    /// Fast breathing while resetting (300 ms, peak brightness).
    Resetting,
    /// Solid on at the configured brightness.
    Ok,
    /// Slow blink at the configured brightness (1000 ms period).
    Warning,
    /// Fast blink at peak brightness (200 ms period).
    Error,
}

impl LedStatus {
    /// Stable wire encoding for the status atomic.
    const fn code(self) -> u8 {
        match self {
            Self::Off => 0,
            Self::Startup => 1,
            Self::Resetting => 2,
            Self::Ok => 3,
            Self::Warning => 4,
            Self::Error => 5,
        }
    }

    /// Decode a stored status code; unknown codes yield `None`.
    const fn from_code(code: u8) -> Option<Self> {
        match code {
            0 => Some(Self::Off),
            1 => Some(Self::Startup),
            2 => Some(Self::Resetting),
            3 => Some(Self::Ok),
            4 => Some(Self::Warning),
            5 => Some(Self::Error),
            _ => None,
        }
    }
}

/// Requested status; written by [`set`], consumed by [`task`].
static STATUS: AtomicU8 = AtomicU8::new(0);
/// Config-driven brightness (0–255) for [`LedStatus::Ok`] / [`LedStatus::Warning`].
static BRIGHTNESS: AtomicU8 = AtomicU8::new(DEFAULT_BRIGHTNESS);

/// Selects the status pattern.
///
/// Lock-free (`AtomicU8` store), safe to call from any task or interrupt.
/// The effect loop restarts the pattern phase on the next tick, so blink
/// and breathing always start from their initial position.
pub fn set(status: LedStatus) {
    STATUS.store(status.code(), Ordering::Release);
}

/// Sets the config-driven brightness (0–255) used by [`LedStatus::Ok`] and
/// [`LedStatus::Warning`].
///
/// Pushed by [`crate::config::Config::apply`]; the default matches
/// `Config::default().led_brightness`.
pub fn set_brightness(brightness: u8) {
    BRIGHTNESS.store(brightness, Ordering::Release);
}

/// Runs the status-LED effect loop, owning the LEDC peripheral.
///
/// Configure the LEDC timer/channel once, then apply [`set`] statuses at
/// will — the loop picks up every change on its next 10 ms tick.
///
/// # Panics
///
/// Panics if the LEDC timer or channel fails to configure (should not
/// happen on a healthy peripheral).
#[embassy_executor::task]
pub async fn task(ledc: LEDC<'static>, pin: GPIO7<'static>) -> ! {
    let mut ledc = Ledc::new(ledc);
    ledc.set_global_slow_clock(LSGlobalClkSource::APBClk);

    let mut timer0 = ledc.timer::<LowSpeed>(timer::Number::Timer0);
    timer0
        .configure(timer::config::Config {
            duty: timer::config::Duty::Duty14Bit,
            clock_source: LSClockSource::APBClk,
            frequency: Rate::from_khz(1),
        })
        .expect("LEDC timer0 config failed");

    let mut channel = ledc.channel(channel::Number::Channel0, pin);
    channel
        .configure(channel::config::Config {
            timer: &timer0,
            duty_pct: 0,
            drive_mode: esp_hal::gpio::DriveMode::PushPull,
        })
        .expect("LEDC channel0 config failed");

    let mut current = current_status();
    let mut phase = Instant::now();

    loop {
        let status = current_status();
        if status != current {
            current = status;
            phase = Instant::now();
        }
        let brightness = BRIGHTNESS.load(Ordering::Acquire);
        apply(&channel, status, brightness, phase.elapsed().as_millis());
        Timer::after(TICK).await;
    }
}

/// Reads the requested status; unknown codes fall back to [`LedStatus::Off`].
fn current_status() -> LedStatus {
    LedStatus::from_code(STATUS.load(Ordering::Acquire)).unwrap_or(LedStatus::Off)
}

/// Applies `status` to the LEDC channel.
///
/// # Arguments
/// * `channel` - The LEDC PWM channel to control.
/// * `status` - The requested status pattern.
/// * `config_brightness` - Config-driven brightness (0–255) for the states
///   that use it ([`LedStatus::Ok`], [`LedStatus::Warning`]).
/// * `elapsed_ms` - Milliseconds since the last status change; every period
///   starts over from zero (blink begins on, breathing begins dark).
fn apply(
    channel: &channel::Channel<'_, LowSpeed>,
    status: LedStatus,
    config_brightness: u8,
    elapsed_ms: u64,
) {
    match status {
        LedStatus::Off => {
            channel.set_duty_hw(0);
        }
        LedStatus::Startup => {
            breathing(channel, PEAK_BRIGHTNESS, elapsed_ms, STARTUP_PERIOD_MS);
        }
        LedStatus::Resetting => {
            breathing(channel, PEAK_BRIGHTNESS, elapsed_ms, RESETTING_PERIOD_MS);
        }
        LedStatus::Ok => {
            channel.set_duty_hw(u32::from(brightness_counts(config_brightness)));
        }
        LedStatus::Warning => {
            blink(channel, config_brightness, elapsed_ms, WARNING_PERIOD_MS);
        }
        LedStatus::Error => {
            blink(channel, PEAK_BRIGHTNESS, elapsed_ms, ERROR_PERIOD_MS);
        }
    }
}

/// Breathing: raised sine over `period_ms`, scaled to `brightness`.
///
/// Raised sine `(sinf(2π·t − π/2) + 1) / 2` over `period_ms`, scaled to
/// `brightness` — starts dark, peaks at half the period, ends dark.
fn breathing(
    channel: &channel::Channel<'_, LowSpeed>,
    brightness: u8,
    elapsed_ms: u64,
    period_ms: u64,
) {
    let pos = elapsed_ms.checked_rem(period_ms).unwrap_or(0);
    // Periods fit u16, so the u64 → f32 step stays cast-free.
    let pos_f = f32::from(u16::try_from(pos).unwrap_or(u16::MAX));
    let period_f = f32::from(u16::try_from(period_ms).unwrap_or(u16::MAX));
    let wave = f32::midpoint(libm::sinf(pos_f / period_f * TAU - FRAC_PI_2), 1.0);
    let counts = wave * f32::from(brightness_counts(brightness));
    // The cast is bounded by construction (≤ DUTY_MAX); `core` has no f32 → u32.
    #[allow(
        clippy::as_conversions,
        clippy::cast_possible_truncation,
        clippy::cast_sign_loss
    )]
    channel.set_duty_hw(u32::from(libm::roundf(counts) as u16));
}

/// Blink: on for the first half of `period_ms`, off for the second.
fn blink(
    channel: &channel::Channel<'_, LowSpeed>,
    brightness: u8,
    elapsed_ms: u64,
    period_ms: u64,
) {
    let pos = elapsed_ms.checked_rem(period_ms).unwrap_or(0);
    let on = pos < period_ms / 2;
    channel.set_duty_hw(u32::from(if on {
        brightness_counts(brightness)
    } else {
        0
    }));
}

/// Scale an 8-bit brightness (0–255) to raw duty counts (0–[`DUTY_MAX`]).
///
/// The intermediate math runs in `u32`, which cannot overflow here
/// (`255 * 16383 ≈ 4.2M < u32::MAX`); `saturating_mul` makes that bound
/// explicit for `clippy::arithmetic_side_effects`.
fn brightness_counts(brightness: u8) -> u16 {
    let counts = u32::from(brightness).saturating_mul(u32::from(DUTY_MAX)) / 255;
    u16::try_from(counts).unwrap_or(DUTY_MAX)
}
