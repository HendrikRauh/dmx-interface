//! Panic capture and last-panic persistence.
//!
//! Flow on a panic (or an Xtensa exception, which `esp-hal` turns into a
//! `panic!`):
//!
//! 1. The banner (message, location, backtrace) and the most recent log
//!    lines are moved out of the ring into
//!    `.rtc_slow.persistent` — RTC slow memory, which esp-hal only zeroes
//!    on power-on and preserves across software resets.
//! 2. The same bytes are written to NVS (best effort, skipped when the
//!    flash is busy or storage is down — the RTC copy is the fallback).
//! 3. The boot-loop counter increments; after [`MAX_PANICS`] panics without
//!    a 30 s stable run (see `mark_stable`) the firmware halts instead of
//!    resetting. Otherwise it reboots.
//!
//! On the next boot [`report_last_panic`] replays the stored bytes into the
//! log ring, so `inv monitor` shows the previous crash as soon as a host
//! connects. Live output during the panic itself is impossible: the USB
//! transfer pipeline is owned by the embassy executor, which a panic
//! handler cannot drive.
//!
//! The report stays in NVS/RTC until the next panic overwrites it — the web
//! UI can later expose it as an endpoint.

use core::cell::{Cell, UnsafeCell};
use core::panic::PanicInfo;

use esp_backtrace::Backtrace;

use crate::{logging, storage};

/// Magic marking a valid RTC snapshot.
///
/// Deterministic: esp-hal zeroes `.rtc_slow.persistent` on power-on, so a
/// garbage match cannot happen — after a power cycle this is `0`.
const RTC_MAGIC: u32 = 0x4358_5050;

/// Panic payload staged in RTC slow memory (12-byte header + data; RTC slow
/// has 8 KiB in total).
const SNAPSHOT_LEN: usize = 3072;

/// Consecutive panics without a stable run after which the firmware halts
/// instead of resetting (boot-loop protection).
const MAX_PANICS: usize = 3;

/// Guards against a panic while panicking (would otherwise recurse).
///
/// A `critical_section`-guarded cell instead of an `AtomicBool`: xtensa has
/// no compare-and-swap, so `AtomicBool::swap` does not exist on this target.
static IN_PANIC: critical_section::Mutex<Cell<bool>> =
    critical_section::Mutex::new(Cell::new(false));

/// Snapshot layout in `.rtc_slow.persistent`.
#[repr(C)]
struct RtcSnapshot {
    magic: u32,
    len: usize,
    panic_count: usize,
    data: [u8; SNAPSHOT_LEN],
}

/// Wrapper making [`RtcSnapshot`] a `'static` interior-mutable cell (avoids
/// `static mut`, which edition 2024 rejects references to).
struct RtcCell(UnsafeCell<RtcSnapshot>);

// SAFETY: the cell is only touched from the panic handler and from boot-time
// code; both run on the same core and never concurrently (panic handler
// re-entry is blocked by `IN_PANIC`).
unsafe impl Sync for RtcCell {}

/// The persistent snapshot. esp-hal zeroes this section on power-on only,
/// so the contents survive the software reset issued below.
#[unsafe(link_section = ".rtc_slow.persistent")]
static RTC: RtcCell = RtcCell(UnsafeCell::new(RtcSnapshot {
    magic: 0,
    len: 0,
    panic_count: 0,
    data: [0; SNAPSHOT_LEN],
}));

/// The `#[panic_handler]` of this firmware (esp-backtrace's built-in
/// handler is disabled in `Cargo.toml` so the report can be captured here).
#[panic_handler]
fn panic_handler(info: &PanicInfo<'_>) -> ! {
    let already_in_panic = critical_section::with(|cs| {
        let flag = IN_PANIC.borrow(cs);
        let prev = flag.get();
        flag.set(true);
        prev
    });
    if already_in_panic {
        // Panic inside panic handling: do not touch memory again.
        halt();
    }

    logging::push_line('!', format_args!("PANIC: {info}"));
    if let Some(location) = info.location() {
        logging::push_line('!', format_args!("at {location}"));
    }
    logging::push_line('!', format_args!("backtrace:"));
    let backtrace = Backtrace::capture();
    for frame in backtrace.frames() {
        logging::push_line('!', format_args!("  0x{:08x}", frame.program_counter()));
    }

    let panic_count = {
        // SAFETY: re-entry is blocked by `IN_PANIC` and no other code runs
        // on this core while the handler is active.
        let snapshot = unsafe { &mut *RTC.0.get() };
        let previous = if snapshot.magic == RTC_MAGIC {
            snapshot.panic_count
        } else {
            0
        };
        snapshot.magic = RTC_MAGIC;
        snapshot.len = logging::drain_snapshot(&mut snapshot.data);
        let count = previous.wrapping_add(1);
        snapshot.panic_count = count;

        // Durable copy; skipped when the flash is busy or storage is down
        // (the RTC copy above is then the only record).
        storage::persist_panic(snapshot.data.get(..snapshot.len).unwrap_or(&[]));
        count
    };

    if panic_count >= MAX_PANICS {
        halt();
    }
    esp_hal::system::software_reset();
}

/// Spin forever with interrupts masked.
///
/// Used for boot-loop protection and for a panic during panic handling.
fn halt() -> ! {
    critical_section::with(|_| -> ! {
        loop {
            core::hint::spin_loop();
        }
    })
}

/// Clear the boot-loop counter; called once the firmware ran stably for
/// 30 s (see the `stable_marker` task in `main`).
pub fn mark_stable() {
    // SAFETY: single-core boot-time access; a panic cannot interleave with
    // this on another core and re-entry is guarded.
    let snapshot = unsafe { &mut *RTC.0.get() };
    snapshot.panic_count = 0;
}

/// Replay the persisted last-panic report into the log ring.
///
/// Prefers the RTC snapshot: it is written on every panic (it cannot
/// fail) and only zeroed on power-on, so whenever its magic checks out it
/// is at least as fresh as the NVS copy — which is skipped while flash is
/// busy, e.g. a panic during a config save. NVS is the fallback after a
/// power cycle. Called once at startup, before the USB task is spawned;
/// the ring buffers the report until a host connects. Nothing is cleared —
/// the next panic overwrites it.
pub fn report_last_panic() {
    // SAFETY: boot path on a single core; the panic handler has not
    // run yet this boot.
    let snapshot = unsafe { &*RTC.0.get() };
    let data = if snapshot.magic == RTC_MAGIC && snapshot.len > 0 && snapshot.len <= SNAPSHOT_LEN {
        log::debug!("RTC panic snapshot ({} bytes)", snapshot.len);
        snapshot.data.get(..snapshot.len).map(<[u8]>::to_vec)
    } else {
        storage::load_panic()
    };
    if let Some(data) = data {
        log::error!("=== LAST PANIC (previous run) ===");
        // Verbatim: the stored lines already carry their timestamps and
        // markers, and re-splitting them would lose the line breaks.
        logging::push_raw(&data);
    }
    // Snapshots taken later this boot must not contain the replayed bytes —
    // otherwise each stored report would swallow the previous one.
    logging::mark_snapshot_floor();
}
