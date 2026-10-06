//! DMX interface firmware for ESP32-S2 (Lolin S2 Mini).
//!
//! Current scope: LED effect on GPIO7 (LEDC PWM), USB-CDC with esptool
//! auto-reset, and a diagnostic blink on the onboard LED (GPIO15).

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
/// NVS-backed persistent storage.
mod storage;

use embassy_executor::Spawner;
use embassy_time::{Duration, Instant, Timer};
use esp_alloc::HeapRegion;
use esp_backtrace as _;
use esp_hal::{
    clock::CpuClock,
    gpio::{Level, Output, OutputConfig},
    interrupt::software::SoftwareInterruptControl,
    ledc::{
        LSGlobalClkSource, Ledc, LowSpeed,
        channel::{self, ChannelIFace},
        timer::{self, LSClockSource, TimerIFace},
    },
    time::Rate,
    timer::timg::TimerGroup,
};
use esp_println::println;

use hardware::led::LedEffect;

esp_bootloader_esp_idf::esp_app_desc!();

/// Entry point.
#[esp_rtos::main]
async fn main(spawner: Spawner) -> ! {
    let peripherals = esp_hal::init(esp_hal::Config::default().with_cpu_clock(CpuClock::max()));

    // ── Heap allocator ─────────────────────────────────────────────────────
    {
        /// Heap size in bytes (8 KiB internal RAM).
        const HEAP_SIZE: usize = 8 * 1024;
        /// Backing memory for the heap allocator.
        static mut HEAP_MEM: [u8; HEAP_SIZE] = [0u8; HEAP_SIZE];
        unsafe {
            esp_alloc::HEAP.add_region(HeapRegion::new(
                core::ptr::addr_of_mut!(HEAP_MEM) as *mut u8,
                HEAP_SIZE,
                esp_alloc::MemoryCapability::Internal.into(),
            ));
        }
    }

    // ── Embassy scheduler ──────────────────────────────────────────────────
    let timg0 = TimerGroup::new(peripherals.TIMG0);
    let sw_int = SoftwareInterruptControl::new(peripherals.SW_INTERRUPT);
    esp_rtos::start(timg0.timer0, sw_int.software_interrupt0);

    // ── USB-CDC (esptool auto-reset) ───────────────────────────────────────
    spawner.spawn(
        hardware::usb::task(peripherals.USB0, peripherals.GPIO20, peripherals.GPIO19)
            .expect("USB task spawn failed"),
    );

    // ── LEDC PWM (status LED) ──────────────────────────────────────────────
    let mut ledc = Ledc::new(peripherals.LEDC);
    ledc.set_global_slow_clock(LSGlobalClkSource::APBClk);

    let mut timer0 = ledc.timer::<LowSpeed>(timer::Number::Timer0);
    timer0
        .configure(timer::config::Config {
            duty: timer::config::Duty::Duty14Bit,
            clock_source: LSClockSource::APBClk,
            frequency: Rate::from_khz(1),
        })
        .unwrap();

    let mut led = ledc.channel(channel::Number::Channel0, peripherals.GPIO7);
    led.configure(channel::config::Config {
        timer: &timer0,
        duty_pct: 0,
        drive_mode: esp_hal::gpio::DriveMode::PushPull,
    })
    .unwrap();

    println!("Init complete — LED blink");

    // Onboard LED (GPIO15): 1 Hz blink = main loop is alive (no serial out).
    let mut diag = Output::new(peripherals.GPIO15, Level::Low, OutputConfig::default());
    let mut diag_ticks = 0u32;

    let effect = LedEffect::Blinking;
    let start = Instant::now();

    loop {
        let elapsed = start.elapsed().as_millis() as u64;
        hardware::led::apply(&mut led, &effect, 100, elapsed);
        diag_ticks += 1;
        if diag_ticks % 50 == 0 {
            diag.toggle();
        }
        Timer::after(Duration::from_millis(10)).await;
    }
}
