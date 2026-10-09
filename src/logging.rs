//! Logging backend: routes the `log-04` facade into a static ring buffer
//! which is drained over the USB-CDC connection.
//!
//! Architecture:
//!
//! ```text
//! log::info!(...) ─► Logger ─► push_line() ─► CDC ring (4 KiB, drop-oldest)
//!                                    ▲
//!                        panic_report (raw, bypasses level filter)
//!
//! usb.rs class_task: cdc_peek ─► write_packet ─► cdc_commit ─► inv monitor
//! ```
//!
//! The ring is a static, allocation-free byte buffer. Producers append
//! formatted lines (oldest bytes are overwritten when full), the single
//! consumer in the USB class task copies chunks out with peek/commit so a
//! cancelled write never loses data.

use core::cell::UnsafeCell;
use core::fmt::Write as _;

use embassy_time::Instant;

/// Total CDC ring capacity in bytes (power of two).
const RING_SIZE: usize = 4096;
/// Maximum formatted length of one log line, prefix and newline included.
const LINE_MAX: usize = 256;

/// Set once [`init`] ran; gates embassy-time access (the time driver only
/// starts with `esp_rtos::start()`, before that `Instant::now()` would panic).
static READY: core::sync::atomic::AtomicBool = core::sync::atomic::AtomicBool::new(false);

/// Global CDC ring: shared by every producer and the USB drain task.
static CDC_RING: LogRing<RING_SIZE> = LogRing::new();

/// `log::Log` implementation that formats records into [`CDC_RING`].
struct Logger;

/// Static logger instance registered by [`init`].
static LOGGER: Logger = Logger;

/// Initialize the global logger.
///
/// Must be called after `esp_rtos::start()` (uptime timestamps need the
/// embassy time driver). The max level comes from the `LOG_LEVEL` env var
/// baked in by `build.rs` (debug profile: debug, release profile: info).
pub fn init() {
    // The racy variants are required: xtensa has no CAS, so the plain
    // `set_logger`/`set_max_level` functions do not exist on this target.
    critical_section::with(|_| {
        // SAFETY: invoked exactly once at startup with interrupts masked by
        // the critical section, so no other `set_logger` call can race (the
        // safety contract of the racy function).
        unsafe {
            log::set_logger_racy(&LOGGER).expect("logger already installed");
        }
        // SAFETY: same single, interrupt-masked startup call as above, no
        // concurrent `set_max_level` possible.
        unsafe {
            log::set_max_level_racy(configured_level());
        }
    });
    READY.store(true, core::sync::atomic::Ordering::Relaxed);
}

/// Map the `LOG_LEVEL` build env to a max level; unknown values fall back to Info.
fn configured_level() -> log::LevelFilter {
    match env!("LOG_LEVEL") {
        "error" => log::LevelFilter::Error,
        "warn" => log::LevelFilter::Warn,
        "debug" => log::LevelFilter::Debug,
        "trace" => log::LevelFilter::Trace,
        _ => log::LevelFilter::Info,
    }
}

/// One-character level marker used in the line prefix.
const fn level_marker(level: log::Level) -> char {
    match level {
        log::Level::Error => 'E',
        log::Level::Warn => 'W',
        log::Level::Info => 'I',
        log::Level::Debug => 'D',
        log::Level::Trace => 'T',
    }
}

/// Milliseconds since boot; `0` before the embassy time driver runs.
fn uptime_ms() -> u64 {
    if READY.load(core::sync::atomic::Ordering::Relaxed) {
        Instant::now().as_millis()
    } else {
        0
    }
}

/// Format one prefixed line (`[<uptime>ms][<marker>] <args>\n`) into the CDC
/// ring. Used by the [`log::Log`] implementation and by `panic_report`
/// (which bypasses the log level filter).
///
/// Long lines are truncated but always keep a trailing newline, so the next
/// line starts clean. Never allocates and never panics.
pub(crate) fn push_line(marker: char, args: core::fmt::Arguments<'_>) {
    let mut buf = heapless::String::<LINE_MAX>::new();
    let ms = uptime_ms();
    let _ = write!(buf, "[{ms:>7}ms][{marker}] ");
    let _ = core::fmt::write(&mut buf, args);
    if buf.push('\n').is_err() {
        buf.pop();
        let _ = buf.push('\n');
    }
    CDC_RING.push(buf.as_bytes());
}

/// Copy as much pending data as fits into `out` without consuming it.
///
/// Returns the number of bytes copied (0 when the ring is empty).
pub(crate) fn cdc_peek(out: &mut [u8]) -> usize {
    CDC_RING.peek(out)
}

/// Mark the `n` bytes previously returned by [`cdc_peek`] as consumed.
pub(crate) fn cdc_commit(n: usize) {
    CDC_RING.commit(n);
}

/// Move the ring contents since [`mark_snapshot_floor`] into `out`,
/// dropping the oldest bytes if needed so the newest data always fits.
/// Consumes everything.
///
/// Used by the panic handler to snapshot the panic banner plus the log
/// lines that followed the boot-time replay for persistence.
pub(crate) fn drain_snapshot(out: &mut [u8]) -> usize {
    CDC_RING.drain_snapshot(out)
}

/// Append bytes verbatim to the CDC ring (no prefix, no newline handling).
///
/// Used by the boot-time replay of a persisted panic report, whose lines
/// already carry their own timestamp/marker.
pub(crate) fn push_raw(bytes: &[u8]) {
    CDC_RING.push(bytes);
}

/// Mark the current write position as the earliest byte a panic snapshot
/// may contain, called after the boot-time replay.
///
/// Without this, [`drain_snapshot`] would capture the replayed report of
/// the previous crash too, and every stored report would swallow its
/// predecessor until the snapshot buffer only held repeats.
pub(crate) fn mark_snapshot_floor() {
    CDC_RING.set_floor();
}

impl log::Log for Logger {
    /// Gate records against the globally configured max level and the
    /// crate's own target namespace.
    ///
    /// The `log-04` features of `esp-hal`/`esp-rtos` route their messages
    /// into the same facade — without filtering, the esp-rtos scheduler
    /// would flood the 4 KiB ring with task-switch traces on every `debug`
    /// build. Only records originating from this crate's module tree are
    /// accepted; third-party targets can be added here deliberately.
    fn enabled(&self, metadata: &log::Metadata<'_>) -> bool {
        metadata.level() <= log::max_level() && metadata.target().starts_with("dmx_interface")
    }

    /// Format matching records as prefixed lines into the ring.
    fn log(&self, record: &log::Record<'_>) {
        if self.enabled(record.metadata()) {
            push_line(level_marker(record.level()), *record.args());
        }
    }

    /// No-op — the ring is drained asynchronously by the USB class task.
    fn flush(&self) {}
}

/// Raw ring contents guarded by [`LogRing`]'s critical sections.
struct State<const N: usize> {
    buf: [u8; N],
    /// Read position, monotonic logical byte cursor (wrapping).
    head: usize,
    /// Write position, monotonic logical byte cursor (wrapping).
    tail: usize,
    /// Earliest logical position a panic snapshot may start at — set once
    /// after the boot-time replay so snapshots never re-capture it.
    floor: usize,
}

/// Single-producer/single-consumer byte ring with overwrite-oldest policy.
///
/// Every operation runs inside one critical section. Positions are logical
/// `usize` cursors masked into the buffer (size must be a power of two), so
/// wrap-around is just masking and dropping the oldest bytes never moves
/// data.
struct LogRing<const N: usize> {
    state: UnsafeCell<State<N>>,
}

// SAFETY: every access to the `UnsafeCell` happens inside a critical section
// (see the method bodies), so the state is never aliased mutably.
unsafe impl<const N: usize> Sync for LogRing<N> {}

impl<const N: usize> LogRing<N> {
    /// Bitmask mapping a logical cursor to a buffer offset (`N - 1`).
    const MASK: usize = {
        assert!(N.is_power_of_two(), "ring size must be a power of two");
        N.wrapping_sub(1)
    };

    /// Create an empty ring; `const fn` so a `static` can build it.
    const fn new() -> Self {
        Self {
            state: UnsafeCell::new(State {
                buf: [0; N],
                head: 0,
                tail: 0,
                floor: 0,
            }),
        }
    }

    /// Number of pending bytes (`tail - head` with wrapping arithmetic).
    fn used(s: &State<N>) -> usize {
        s.tail.wrapping_sub(s.head)
    }

    /// Append `data`, overwriting the oldest bytes when there is not enough
    /// room. Never panics, never allocates.
    fn push(&self, data: &[u8]) {
        if data.is_empty() {
            return;
        }
        critical_section::with(|_| {
            // SAFETY: exclusive access inside the critical section.
            let s = unsafe { &mut *self.state.get() };

            // Keep only the newest N bytes of an oversized input.
            let data = if data.len() >= N {
                data.split_at(data.len().wrapping_sub(N)).1
            } else {
                data
            };

            // Make room by dropping the oldest bytes.
            let free = N.wrapping_sub(Self::used(s));
            if data.len() > free {
                s.head = s.head.wrapping_add(data.len().wrapping_sub(free));
            }

            // Copy in at most two segments (the tail may wrap).
            let off = s.tail & Self::MASK;
            let first = data.len().min(N.wrapping_sub(off));
            let (seg1, seg2) = data.split_at(first);
            let seg2_len = seg2.len();

            let (prefix, rest) = s.buf.split_at_mut(off);
            let (dst1, _) = rest.split_at_mut(seg1.len());
            dst1.copy_from_slice(seg1);
            if seg2_len > 0 {
                let (dst2, _) = prefix.split_at_mut(seg2_len);
                dst2.copy_from_slice(seg2);
            }

            s.tail = s.tail.wrapping_add(data.len());
        });
    }

    /// Copy up to `out.len()` pending bytes into `out` without consuming
    /// them. Returns how many were copied.
    fn peek(&self, out: &mut [u8]) -> usize {
        if out.is_empty() {
            return 0;
        }
        critical_section::with(|_| {
            // SAFETY: shared access inside the critical section.
            let s = unsafe { &*self.state.get() };
            let n = Self::used(s).min(out.len());
            let off = s.head & Self::MASK;
            let first = n.min(N.wrapping_sub(off));
            let remaining = n.wrapping_sub(first);

            let (prefix, rest) = s.buf.split_at(off);
            let (src1, _) = rest.split_at(first);
            let (dst1, dst2) = out.split_at_mut(first);
            dst1.copy_from_slice(src1);
            if remaining > 0 {
                let (src2, _) = prefix.split_at(remaining);
                let (dst2, _) = dst2.split_at_mut(remaining);
                dst2.copy_from_slice(src2);
            }
            n
        })
    }

    /// Mark the `n` most recently peeked bytes as consumed. Values larger
    /// than the pending amount consume everything.
    fn commit(&self, n: usize) {
        critical_section::with(|_| {
            // SAFETY: exclusive access inside the critical section.
            let s = unsafe { &mut *self.state.get() };
            let used = Self::used(s);
            s.head = if n >= used {
                s.tail
            } else {
                s.head.wrapping_add(n)
            };
        });
    }

    /// Copy the newest bytes into `out`, then empty the ring. Returns how
    /// many bytes were written (oldest data is dropped first when `out` is
    /// too small).
    ///
    /// Bytes before the [`floor`](Self::set_floor) marker are skipped: a
    /// snapshot must contain the panic banner plus the log lines that
    /// followed the boot-time replay, never the replayed report itself.
    fn drain_snapshot(&self, out: &mut [u8]) -> usize {
        if out.is_empty() {
            return 0;
        }
        critical_section::with(|_| {
            // SAFETY: exclusive access inside the critical section.
            let s = unsafe { &mut *self.state.get() };
            let start = s.head.max(s.floor);
            let avail = s.tail.wrapping_sub(start);
            let skip = avail.saturating_sub(out.len());
            let n = avail.wrapping_sub(skip);
            let begin = start.wrapping_add(skip);
            let off = begin & Self::MASK;
            let first = n.min(N.wrapping_sub(off));
            let remaining = n.wrapping_sub(first);

            let (prefix, rest) = s.buf.split_at(off);
            let (src1, _) = rest.split_at(first);
            let (dst1, dst2) = out.split_at_mut(first);
            dst1.copy_from_slice(src1);
            if remaining > 0 {
                let (src2, _) = prefix.split_at(remaining);
                let (dst2, _) = dst2.split_at_mut(remaining);
                dst2.copy_from_slice(src2);
            }

            s.head = s.tail;
            n
        })
    }

    /// Mark the current write position as the earliest byte
    /// [`drain_snapshot`](Self::drain_snapshot) may capture.
    fn set_floor(&self) {
        critical_section::with(|_| {
            // SAFETY: exclusive access inside the critical section.
            let s = unsafe { &mut *self.state.get() };
            s.floor = s.tail;
        });
    }
}
