//! DMX interface firmware for ESP32-S2 (Lolin S2 Mini).
//!
//! Current scope: status LED on GPIO7 (LEDC PWM, own embassy task), USB-CDC
//! with esptool auto-reset, ring-buffer logging over the CDC port, and
//! panic reports persisted across resets.

#![no_std]
#![no_main]
#![deny(missing_docs)]
#![allow(dead_code)]

extern crate alloc;

/// Board pin definitions (cfg-gated per target).
mod boards;
/// Configuration data model.
mod config;
/// Hardware abstraction (LED, button, eFuse, USB).
mod hardware;
/// Log ring buffer drained over the USB-CDC connection.
mod logging;
/// Panic capture and last-panic persistence.
mod panic_report;
/// NVS-backed persistent storage.
mod storage;

use embassy_executor::Spawner;
use embassy_time::{Duration, Timer};
use esp_alloc::HeapRegion;
use esp_hal::{
    clock::CpuClock, interrupt::software::SoftwareInterruptControl, timer::timg::TimerGroup,
};

use hardware::led::LedStatus;

esp_bootloader_esp_idf::esp_app_desc!();

/// Clears the panic boot-loop counter once the firmware ran stably for 30 s.
#[embassy_executor::task]
async fn stable_marker() {
    Timer::after(Duration::from_secs(30)).await;
    panic_report::mark_stable();
}

/// Entry point.
#[esp_rtos::main]
async fn main(spawner: Spawner) -> ! {
    let peripherals = esp_hal::init(esp_hal::Config::default().with_cpu_clock(CpuClock::max()));

    // ── Heap allocator ─────────────────────────────────────────────────────
    {
        /// Heap size in bytes (32 KiB internal RAM — the panic-report replay
        /// and NVS blob reads allocate, 8 KiB was too tight).
        const HEAP_SIZE: usize = 32 * 1024;
        /// Backing memory for the heap allocator.
        static mut HEAP_MEM: [u8; HEAP_SIZE] = [0u8; HEAP_SIZE];
        let heap_ptr = core::ptr::addr_of_mut!(HEAP_MEM).cast::<u8>();
        // SAFETY: `heap_ptr` points to the whole `HEAP_MEM` array, which is
        // valid for `'static`, exclusively owned by the allocator afterwards,
        // handed over exactly once, and `size > 0`.
        let region = unsafe {
            HeapRegion::new(
                heap_ptr,
                HEAP_SIZE,
                esp_alloc::MemoryCapability::Internal.into(),
            )
        };
        // SAFETY: the heap region is registered exactly once at init time,
        // before anything allocates.
        unsafe {
            esp_alloc::HEAP.add_region(region);
        }
    }

    // ── Embassy scheduler ──────────────────────────────────────────────────
    let timg0 = TimerGroup::new(peripherals.TIMG0);
    let sw_int = SoftwareInterruptControl::new(peripherals.SW_INTERRUPT);
    esp_rtos::start(timg0.timer0, sw_int.software_interrupt0);

    // ── Logging + last-panic report ────────────────────────────────────────
    // Must follow esp_rtos::start (uptime timestamps need the time driver)
    // and precede the USB spawn, so a stored panic report is already in the
    // ring when the first host connects.
    logging::init();
    storage::init(peripherals.FLASH);
    // Load the persisted config and push it into every consumer (LED brightness).
    storage::load().apply();
    panic_report::report_last_panic();

    // ── USB-CDC (esptool auto-reset + log drain) ───────────────────────────
    spawner.spawn(
        hardware::usb::task(peripherals.USB0, peripherals.GPIO20, peripherals.GPIO19)
            .expect("USB task spawn failed"),
    );
    spawner.spawn(stable_marker().expect("stable marker spawn failed"));

    // ── Status LED (LEDC PWM, own task) ─────────────────────────────────────
    hardware::led::set(LedStatus::Startup);
    spawner.spawn(
        hardware::led::task(peripherals.LEDC, peripherals.GPIO7).expect("LED task spawn failed"),
    );

    log::info!("Init complete");

    // TODO(phase 6): switch to Ok only once the network is up — the old
    // firmware waited for NETWORK_READY + 2 s before LED_MODE_NORMAL.
    // Until then, keep the Startup breathing visible for 3 s (three full
    // breaths at the 1 s period) before settling into the steady Ok state.
    Timer::after(Duration::from_secs(3)).await;
    hardware::led::set(LedStatus::Ok);

    // No main-loop duties yet — Phase 8 moves system work into tasks.
    loop {
        core::future::pending::<()>().await;
    }
}
