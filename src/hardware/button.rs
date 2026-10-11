//! User button (GPIO5, active-low): power-up factory reset and runtime
//! click actions.
//!
//! One pin, two phases. [`boot_factory_reset`] runs first, awaited straight
//! from `main` before the network task is spawned (but only after a
//! non-software reset — a reboot we triggered ourselves must never re-enter
//! the hold while the button is still down):
//!
//! | Phase | Trigger | Behavior |
//! | ----- | ------- | --------- |
//! | Boot hold | held at power-up | [`LedStatus::Resetting`] for [`RESET_HOLD`], then [`storage::clear`] → solid LED → wait for release → reboot; releasing earlier aborts with [`LedStatus::Warning`] |
//! | Click actions | presses after boot | classified into [`ButtonEvent`]s, dispatched from `Config::button_actions` |
//!
//! Event discriminants are the `button_actions` map keys (web: `ButtonEvent`):
//! 0 single click, 1 double click, 2 multiple (3) clicks, 3 long hold. The
//! action is read from NVS on every event, so the web UI can rebind clicks
//! without a reboot.

use embassy_time::{Duration, Instant, Timer};
use esp_hal::{
    gpio::{Input, InputConfig, Pull},
    peripherals::GPIO5,
    rtc_cntl::SocResetReason,
    system::reset_reason,
};

use crate::{
    config::ButtonAction,
    hardware::led::{self, LedStatus},
    storage,
};

/// Poll interval of the boot hold check and the click classifier.
const POLL: Duration = Duration::from_millis(10);

/// A raw level must hold this long before the stable level follows it.
const DEBOUNCE: Duration = Duration::from_millis(30);

/// Power-up hold before the factory reset fires.
const RESET_HOLD: Duration = Duration::from_secs(3);

/// Maximum gap between two presses of one click sequence.
const CLICK_GAP: Duration = Duration::from_millis(600);

/// Press duration that fires [`ButtonEvent::LongHold`].
const LONG_HOLD: Duration = Duration::from_millis(1500);

/// Progress-logging cadence while the boot hold runs.
const HOLD_LOG_EVERY: Duration = Duration::from_millis(500);

/// Delay before a button-triggered reboot so the last log line drains.
const REBOOT_DELAY: Duration = Duration::from_millis(100);

/// Brightness ladder of [`ButtonAction::ToggleLed`] (legacy behavior).
const BRIGHTNESS_LEVELS: [u8; 5] = [0, 1, 20, 100, 255];

/// Button event classified at runtime.
///
/// The discriminant is the key into `Config::button_actions` (web:
/// `ButtonEvent`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ButtonEvent {
    /// One press and release (`button_actions` key 0).
    SingleClick = 0,
    /// Two presses within [`CLICK_GAP`] (key 1).
    DoubleClick = 1,
    /// Three presses within [`CLICK_GAP`] (key 2).
    MultipleClick = 2,
    /// Held for at least [`LONG_HOLD`] (key 3).
    LongHold = 3,
}

impl From<ButtonEvent> for u8 {
    /// Event → `button_actions` map key.
    fn from(event: ButtonEvent) -> Self {
        match event {
            ButtonEvent::SingleClick => 0,
            ButtonEvent::DoubleClick => 1,
            ButtonEvent::MultipleClick => 2,
            ButtonEvent::LongHold => 3,
        }
    }
}

/// Configures `pin` as the user-button input (pull-up, active low).
pub fn input(pin: GPIO5<'static>) -> Input<'static> {
    Input::new(pin, InputConfig::default().with_pull(Pull::Up))
}

/// Power-up factory-reset hold (README: hold while applying power).
///
/// Returns immediately after a software-triggered boot (WS `reset`,
/// [`ButtonAction::Reboot`], panic recovery) or when the button is not
/// held. Otherwise the LED shows [`LedStatus::Resetting`] until
/// [`RESET_HOLD`] elapses, the config is cleared, the LED turns solid
/// ([`LedStatus::Ok`]) and the function waits for the release before
/// rebooting. Releasing before the deadline aborts with
/// [`LedStatus::Warning`] and boot continues.
///
/// Call after `storage::init` and the LED task are running. On a
/// successful reset it never returns; if NVS cannot be cleared the reset is
/// aborted (no reboot into a factory reset that did not happen).
pub async fn boot_factory_reset(button: &Input<'_>) {
    let reason = reset_reason();
    if matches!(
        reason,
        Some(SocResetReason::CoreSw | SocResetReason::Cpu0Sw)
    ) {
        log::debug!("button: software-triggered boot, hold check skipped");
        return;
    }
    log::debug!("button: boot reset reason {reason:?}");
    if !button.is_low() {
        log::debug!("button: not held at power-up");
        return;
    }
    Timer::after(DEBOUNCE).await;
    if !button.is_low() {
        log::debug!("button: bounce at power-up, continuing boot");
        return;
    }

    log::warn!(
        "button: held at power-up — factory reset in {} ms",
        RESET_HOLD.as_millis()
    );
    led::set(LedStatus::Resetting);
    let held_since = Instant::now();
    let mut last_log = Instant::now();
    let mut debouncer = Debouncer::new(true);
    loop {
        let pressed = debouncer.update(button.is_low(), Instant::now());
        if !pressed {
            log::info!(
                "button: released after {} ms — factory reset aborted",
                held_since.elapsed().as_millis()
            );
            led::set(LedStatus::Warning);
            return;
        }
        let held = held_since.elapsed();
        if held >= RESET_HOLD {
            break;
        }
        if last_log.elapsed() >= HOLD_LOG_EVERY {
            last_log = Instant::now();
            log::info!("button: still held ({} ms)", held.as_millis());
        }
        Timer::after(POLL).await;
    }

    if !storage::clear() {
        log::error!("button: factory reset failed to clear NVS — not rebooting");
        led::set(LedStatus::Error);
        wait_release(button).await;
        return;
    }
    log::warn!("button: config cleared — release the button to reboot");
    led::set(LedStatus::Ok);
    wait_release(button).await;
    Timer::after(REBOOT_DELAY).await;
    esp_hal::system::software_reset();
}

/// Runs the configured action for `event`, read fresh from NVS.
async fn dispatch(event: ButtonEvent) {
    let mut config = storage::load();
    let Some(action) = config.button_actions.get(&u8::from(event)).copied() else {
        log::info!("button: {event:?} — no action configured");
        return;
    };
    match action {
        ButtonAction::None => {
            log::debug!("button: {event:?} → none");
        }
        ButtonAction::ToggleLed => {
            config.led_brightness = next_brightness(config.led_brightness);
            if storage::save(&config) {
                config.apply();
                log::info!(
                    "button: {event:?} → LED brightness {}",
                    config.led_brightness
                );
            } else {
                log::error!("button: {event:?} → ToggleLed, config save failed");
            }
        }
        ButtonAction::Reboot => {
            log::info!("button: {event:?} → reboot");
            Timer::after(REBOOT_DELAY).await;
            esp_hal::system::software_reset();
        }
    }
}

/// Next [`BRIGHTNESS_LEVELS`] step above `current`, wrapping to the
/// dimmest level after the brightest one.
fn next_brightness(current: u8) -> u8 {
    BRIGHTNESS_LEVELS
        .iter()
        .copied()
        .find(|&level| level > current)
        .unwrap_or(0)
}

/// Classifies presses and dispatches the configured actions.
///
/// Owns the button pin for the rest of the firmware; the boot phase runs
/// earlier, in [`boot_factory_reset`], before this task is spawned.
#[embassy_executor::task]
pub async fn task(button: Input<'static>) -> ! {
    let mut classifier = Classifier::new(button.is_low());
    log::info!("button: click actions armed");
    loop {
        Timer::after(POLL).await;
        if let Some(event) = classifier.update(button.is_low(), Instant::now()) {
            dispatch(event).await;
        }
    }
}

/// Phase of the click sequence.
#[derive(Clone, Copy)]
enum State {
    /// No press in flight.
    Idle,
    /// Debounced press active.
    Holding {
        /// Press start (debounced).
        since: Instant,
        /// Completed presses before this hold.
        clicks: u8,
        /// [`ButtonEvent::LongHold`] already dispatched for this hold.
        long_fired: bool,
    },
    /// Released after `clicks` presses; the next press must arrive within
    /// [`CLICK_GAP`] or the sequence is reported.
    Gap {
        /// Completed presses of the sequence.
        clicks: u8,
        /// Release the click window runs from.
        released_at: Instant,
    },
}

/// Debounced level filter: a raw change must hold for [`DEBOUNCE`] before
/// the stable level follows it.
struct Debouncer {
    /// Raw level of the last poll.
    raw: bool,
    /// When `raw` last changed.
    raw_since: Instant,
    /// Debounced level.
    stable: bool,
}

impl Debouncer {
    /// Seeds both levels with `stable`, the known current level.
    fn new(stable: bool) -> Self {
        Self {
            raw: stable,
            raw_since: Instant::now(),
            stable,
        }
    }

    /// Feeds one poll of the raw level and returns the stable level.
    fn update(&mut self, raw: bool, now: Instant) -> bool {
        if raw != self.raw {
            self.raw = raw;
            self.raw_since = now;
        } else if raw != self.stable && now.saturating_duration_since(self.raw_since) >= DEBOUNCE {
            self.stable = raw;
        }
        self.stable
    }
}

/// Debounced press/hold/click state machine, polled at [`POLL`].
struct Classifier {
    /// Level filter feeding the state machine.
    debouncer: Debouncer,
    /// Current click-sequence phase.
    state: State,
}

impl Classifier {
    /// Creates a classifier synced to the current level.
    ///
    /// A button already held at spawn is treated as an in-progress hold
    /// with [`ButtonEvent::LongHold`] suppressed, so a reboot triggered by
    /// a held button (e.g. `LongHold → Reboot`) waits for the release
    /// before it accepts a new event instead of looping.
    fn new(pressed: bool) -> Self {
        Self {
            debouncer: Debouncer::new(pressed),
            state: if pressed {
                State::Holding {
                    since: Instant::now(),
                    clicks: 0,
                    long_fired: true,
                }
            } else {
                State::Idle
            },
        }
    }

    /// Feeds one poll of the raw level (`pressed` = active-low reading)
    /// and returns the event that completed, if any.
    fn update(&mut self, pressed: bool, now: Instant) -> Option<ButtonEvent> {
        let pressed = self.debouncer.update(pressed, now);
        let (state, event) = match self.state {
            State::Idle => {
                if pressed {
                    (
                        State::Holding {
                            since: now,
                            clicks: 0,
                            long_fired: false,
                        },
                        None,
                    )
                } else {
                    (State::Idle, None)
                }
            }
            State::Holding {
                since,
                clicks,
                long_fired,
            } => {
                if !pressed {
                    let clicks = clicks.saturating_add(1);
                    if long_fired {
                        (State::Idle, None)
                    } else if clicks >= 3 {
                        (State::Idle, Some(ButtonEvent::MultipleClick))
                    } else {
                        (
                            State::Gap {
                                clicks,
                                released_at: now,
                            },
                            None,
                        )
                    }
                } else if !long_fired && since.elapsed() >= LONG_HOLD {
                    (
                        State::Holding {
                            since,
                            clicks,
                            long_fired: true,
                        },
                        Some(ButtonEvent::LongHold),
                    )
                } else {
                    (
                        State::Holding {
                            since,
                            clicks,
                            long_fired,
                        },
                        None,
                    )
                }
            }
            State::Gap {
                clicks,
                released_at,
            } => {
                if released_at.elapsed() >= CLICK_GAP {
                    let event = if clicks <= 1 {
                        ButtonEvent::SingleClick
                    } else {
                        ButtonEvent::DoubleClick
                    };
                    (State::Idle, Some(event))
                } else if pressed {
                    (
                        State::Holding {
                            since: now,
                            clicks,
                            long_fired: false,
                        },
                        None,
                    )
                } else {
                    (
                        State::Gap {
                            clicks,
                            released_at,
                        },
                        None,
                    )
                }
            }
        };
        self.state = state;
        event
    }
}

/// Polls until the button reads released (debounced).
async fn wait_release(button: &Input<'_>) {
    let mut debouncer = Debouncer::new(true);
    while debouncer.update(button.is_low(), Instant::now()) {
        Timer::after(POLL).await;
    }
}
