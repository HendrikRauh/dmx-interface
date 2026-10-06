//! Hardware abstraction for button input.
//!
//! Provides debounced edge detection for momentary push buttons,
//! filtering mechanical bounce with a configurable delay.

use embassy_time::Instant;

/// Default debounce delay in milliseconds.
const DEBOUNCE_MS: u64 = 30;

/// A debounced digital button input.
///
/// Tracks the button state across updates and detects clean press edges,
/// ignoring mechanical bounce within the debounce window.
pub struct DebouncedButton {
    last_press: Instant,
    was_pressed: bool,
}

impl DebouncedButton {
    /// Create a new debounced button in its released state.
    pub fn new() -> Self {
        Self {
            last_press: Instant::now(),
            was_pressed: false,
        }
    }

    /// Poll the button state and return `true` on a clean press edge.
    ///
    /// Call this repeatedly (e.g. in a main loop) with the current
    /// raw GPIO reading (`is_low = true` when pressed).
    ///
    /// # Arguments
    /// * `is_low` - `true` if the button GPIO is currently active (low).
    ///
    /// # Returns
    /// `true` exactly once per press, after the debounce delay has elapsed
    /// since the previous accepted press.
    pub fn update(&mut self, is_low: bool) -> bool {
        let now = Instant::now();
        let pressed_edge = is_low
            && !self.was_pressed
            && now.duration_since(self.last_press).as_millis() >= DEBOUNCE_MS;
        if pressed_edge {
            self.last_press = now;
        }
        self.was_pressed = is_low;
        pressed_edge
    }
}
