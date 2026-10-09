//! NVS persistent storage for device configuration and crash reports.
//!
//! Wraps `esp-nvs` behind a process-wide singleton so the panic handler can
//! persist a crash snapshot even though the FLASH peripheral is consumed at
//! init time. Every operation runs synchronously (no `await` inside) and is
//! serialized through [`NVS_BUSY`], which lets the panic handler decide in
//! one load whether the flash is safe to touch right now.

extern crate alloc;

use alloc::vec::Vec;
use core::cell::{Cell, RefCell};

use critical_section::Mutex;
use esp_hal::peripherals::FLASH;
use esp_nvs::{Key, Nvs};
use esp_storage::FlashStorage;

use crate::config::Config;

/// NVS partition offset (default for ESP32-S2).
const NVS_OFFSET: usize = 0x9000;

/// NVS partition size (default for ESP32-S2: 24 KiB).
const NVS_SIZE: usize = 0x6000;

/// NVS namespace for config storage.
const NS_CONFIG: Key = Key::from_str("config");

/// NVS key for the config blob.
const KEY_CFG: Key = Key::from_str("cfg");

/// NVS namespace for diagnostics.
const NS_DIAG: Key = Key::from_str("diag");

/// NVS key for the persisted last-panic snapshot.
const KEY_PANIC: Key = Key::from_str("last_panic");

/// Concrete storage type: NVS on the internal flash.
type Storage = Nvs<FlashStorage<'static>>;

/// The global handle, set once by [`init`].
static NVS: Mutex<RefCell<Option<Storage>>> = Mutex::new(RefCell::new(None));

/// Set while any NVS operation is in flight.
///
/// * The panic handler skips its durable write when the flag is set (the RTC
///   copy still records the crash) — this both avoids touching a half-used
///   handle and prevents a re-entrant panic from looping on a failing flash.
/// * [`with_nvs`] bails out when it sees the flag already set, so a task
///   interrupted mid-operation can never be re-entered from interrupt/panic
///   context either.
///
/// A `critical_section`-guarded cell instead of an `AtomicBool`: xtensa has
/// no compare-and-swap, so `AtomicBool::swap` does not exist on this target.
static NVS_BUSY: Mutex<Cell<bool>> = Mutex::new(Cell::new(false));

/// Initialize the global NVS handle. Takes ownership of the FLASH peripheral.
///
/// Must be called once at startup (after heap init) before any load/save
/// operation. A failed init leaves storage disabled; operations then fall
/// back to defaults instead of panicking.
pub fn init(flash: FLASH<'static>) {
    match Nvs::new(NVS_OFFSET, NVS_SIZE, FlashStorage::new(flash)) {
        Ok(nvs) => {
            critical_section::with(|cs| {
                *NVS.borrow(cs).borrow_mut() = Some(nvs);
            });
            log::info!("NVS storage initialized ({NVS_SIZE} bytes)");
        }
        Err(e) => log::error!("NVS init failed, storage disabled: {e:?}"),
    }
}

/// Run `f` against the global handle if storage is up and the flash is idle.
///
/// Synchronous by contract: `f` must not `await`, otherwise the busy flag
/// would stay set for the whole suspension. The operation runs inside a
/// critical section — flash writes disable interrupts internally anyway,
/// and it keeps the `RefCell` borrow scoped (`critical_section::Mutex`
/// loans cannot escape the closure).
fn with_nvs<R>(f: impl FnOnce(&mut Storage) -> R) -> Option<R> {
    if busy_swap() {
        return None;
    }
    let out = critical_section::with(|cs| NVS.borrow(cs).borrow_mut().as_mut().map(f));
    busy_clear();
    out
}

/// Atomically set the busy flag and return its previous value.
fn busy_swap() -> bool {
    critical_section::with(|cs| {
        let flag = NVS_BUSY.borrow(cs);
        let prev = flag.get();
        flag.set(true);
        prev
    })
}

/// Clear the busy flag.
fn busy_clear() {
    critical_section::with(|cs| NVS_BUSY.borrow(cs).set(false));
}

/// Load the persisted device configuration.
///
/// Returns [`Config::default`] if nothing is stored, storage is down, or
/// deserialization fails.
pub fn load() -> Config {
    match with_nvs(|nvs| nvs.get::<Vec<u8>>(&NS_CONFIG, &KEY_CFG)) {
        Some(Ok(bytes)) => match Config::from_bytes(&bytes) {
            Ok(config) => {
                log::info!("Config loaded from NVS ({} bytes)", bytes.len());
                config
            }
            Err(e) => {
                log::warn!("Config deserialization failed: {e:?}, using defaults");
                Config::default()
            }
        },
        Some(Err(e)) => {
            log::warn!("NVS read failed: {e:?}, using defaults");
            Config::default()
        }
        None => {
            log::warn!("NVS unavailable, using default config");
            Config::default()
        }
    }
}

/// Save the device configuration to NVS (serialized via `postcard`).
pub fn save(config: &Config) {
    match config.to_bytes() {
        Ok(bytes) => match with_nvs(|nvs| nvs.set(&NS_CONFIG, &KEY_CFG, bytes.as_slice())) {
            Some(Ok(())) => log::info!("Config saved to NVS ({} bytes)", bytes.len()),
            Some(Err(e)) => log::warn!("NVS write failed: {e:?}"),
            None => log::warn!("NVS busy or unavailable, config not saved"),
        },
        Err(e) => log::warn!("Config serialization failed: {e:?}"),
    }
}

/// Clear all config data from NVS (reverts to defaults on next load).
pub fn clear() {
    match with_nvs(|nvs| nvs.delete(&NS_CONFIG, &KEY_CFG)) {
        Some(Ok(())) => log::info!("Config cleared from NVS"),
        Some(Err(e)) => log::warn!("NVS clear failed: {e:?}"),
        None => log::warn!("NVS busy or unavailable, nothing cleared"),
    }
}

/// Persist the last-panic snapshot (overwrites the previous one).
///
/// Returns `false` when storage is down or the flash is busy — the caller
/// then relies on the RTC copy alone.
pub(crate) fn persist_panic(data: &[u8]) -> bool {
    match with_nvs(|nvs| nvs.set(&NS_DIAG, &KEY_PANIC, data)) {
        Some(Ok(())) => true,
        Some(Err(e)) => {
            log::error!("Panic snapshot NVS write failed: {e:?}");
            false
        }
        None => false,
    }
}

/// Load the persisted last-panic snapshot, if any.
///
/// Boot path only (allocates), so the heap must be up.
pub(crate) fn load_panic() -> Option<Vec<u8>> {
    with_nvs(|nvs| nvs.get::<Vec<u8>>(&NS_DIAG, &KEY_PANIC)).and_then(Result::ok)
}
