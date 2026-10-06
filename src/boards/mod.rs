//! Board-specific pin definitions and configuration.
//!
//! Each supported board has its own module with hardware constants.
//! The correct module is re-exported based on the compilation target.

/// Lolin S2 Mini board configuration.
mod s2_mini;

#[cfg(esp32s2)]
pub use s2_mini::*;
