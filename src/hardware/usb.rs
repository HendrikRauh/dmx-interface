//! USB-CDC-ACM device with esptool auto-reset support.
//!
//! The Lolin S2 Mini has no USB-UART bridge, so esptool cannot toggle
//! GPIO0/EN via physical DTR/RTS pins. Instead, the firmware exposes the
//! native USB-OTG port as a CDC-ACM device and interprets the CDC
//! control-line state the way the classic devkit reset sequence does:
//!
//! | DTR | RTS | Meaning            | Action                          |
//! |-----|-----|--------------------|---------------------------------|
//! | 1   | 0   | IO0 low            | set `FORCE_DOWNLOAD_BOOT`       |
//! | 0   | 1   | EN low (reset)     | software reset                  |
//! | 0   | 0   | idle               | clear `FORCE_DOWNLOAD_BOOT`     |
//! | 1   | 1   | transient          | no-op                           |
//!
//! esptool's `--before usb-reset` walk is `(0,0) -> (1,0) -> (0,1) ->
//! (0,0)`: the `(1,0)` state arms `FORCE_DOWNLOAD_BOOT`, then `(0,1)`
//! resets into the ROM bootloader.
//!
//! The flag lives in the RTC domain and survives software resets. The ROM
//! bootloader does **not** clear it on entry — after a flash the chip would
//! stay stuck in the bootloader forever (until power-on). `inv flash`
//! therefore clears the flag itself after flashing (esptool `write-reg
//! 0x3F408128 0 0x1` through the stub) and then resets via the RTC watchdog,
//! which boots the application.
//!
//! The same task also drains the log ring from [`crate::logging`] over the
//! CDC data endpoint, so `inv monitor` shows the firmware log output.

use embassy_futures::{
    join::join,
    select::{Either, select},
};
use embassy_time::{Duration, Instant, Timer};
use embassy_usb::{
    Builder,
    class::cdc_acm::{CdcAcmClass, State},
};
use esp_hal::{
    otg_fs::{
        Usb,
        asynch::{Config, Driver},
    },
    peripherals::{GPIO19, GPIO20, USB0},
};
use static_cell::StaticCell;

use crate::logging;

/// Espressif USB vendor ID (same as the ROM bootloader).
const USB_VID: u16 = 0x303A;

/// Espressif generic CDC-ACM product ID.
const USB_PID: u16 = 0x3001;

/// Line-poll interval of the DTR/RTS state machine.
const POLL_INTERVAL: Duration = Duration::from_millis(10);

/// Log chunk size handed to the CDC endpoint (matches `max_packet_size`).
const LOG_CHUNK: usize = 64;

/// The S2 peripheral types are invariant in their lifetime, which pins the
/// USB driver to `'static`; the embassy-usb buffers therefore must be
/// `'static` too. They are initialized exactly once at task start.
static EP_OUT_BUFFER: StaticCell<[u8; 1024]> = StaticCell::new();
/// Bulk OUT endpoint buffer for the CDC data endpoint.
static CONFIG_DESCRIPTOR: StaticCell<[u8; 256]> = StaticCell::new();
/// Buffer for the generated USB configuration descriptor.
static BOS_DESCRIPTOR: StaticCell<[u8; 256]> = StaticCell::new();
/// Buffer for the generated USB BOS descriptor.
static CONTROL_BUF: StaticCell<[u8; 64]> = StaticCell::new();
/// Scratch buffer for USB control transfers.
static CDC_STATE: StaticCell<State<'static>> = StaticCell::new();

/// Runs the USB-CDC-ACM device and the esptool reset protocol.
///
/// The DP/DM pins are fixed by the S2 hardware (GPIO20/GPIO19).
#[embassy_executor::task]
pub async fn task(usb0: USB0<'static>, dp: GPIO20<'static>, dm: GPIO19<'static>) -> ! {
    let usb = Usb::new(usb0, dp, dm);

    let driver = Driver::new(usb, EP_OUT_BUFFER.init([0u8; 1024]), Config::default());

    let mut config = embassy_usb::Config::new(USB_VID, USB_PID);
    config.manufacturer = Some("dmx-interface");
    config.product = Some("DMX-Interface USB-CDC");
    config.device_class = 0xEF;
    config.device_sub_class = 0x02;
    config.device_protocol = 0x01;
    config.composite_with_iads = true;

    let mut builder = Builder::new(
        driver,
        config,
        CONFIG_DESCRIPTOR.init([0u8; 256]),
        BOS_DESCRIPTOR.init([0u8; 256]),
        &mut [],
        CONTROL_BUF.init([0u8; 64]),
    );
    let mut class = CdcAcmClass::new(&mut builder, CDC_STATE.init(State::new()), 64);
    let mut device = builder.build();

    log::info!("USB-CDC ready");
    join(device.run(), control_and_log(&mut class)).await;
    unreachable!()
}

/// Runs the DTR/RTS state machine and drains the log ring into the host.
///
/// Both roles live in one task because the line poll needs `&class` while
/// the log flush needs `&mut class` — a single `&mut` serves both (`dtr()`
/// and `rts()` take `&self`).
///
/// The future is a `select` of two branches:
///
/// * a deadline-based 10 ms timer that re-checks the control lines (the
///   deadline is only re-armed when the timer actually fired, so log traffic
///   can never starve the esptool reset walk), and
/// * a log flush that copies one chunk out of the ring and awaits
///   `write_packet`. Bytes are only committed after the driver accepted the
///   packet, so a cancelled flush (timer branch won) loses nothing and
///   duplicates nothing.
///
/// The flush has no connection gate: before the host has configured us the
/// endpoint is disabled and the write errors out right away (the flush then
/// waits a poll interval instead of spinning, so the timer branch keeps
/// running), and an idle ring waits 10 ms between polls instead of spinning.
async fn control_and_log(class: &mut CdcAcmClass<'static, Driver<'static>>) -> ! {
    let mut prev = (class.dtr(), class.rts());
    let mut deadline = Instant::now().saturating_add(POLL_INTERVAL);
    loop {
        let outcome = select(Timer::at(deadline), flush_logs(class)).await;
        match outcome {
            Either::First(()) => {
                deadline = Instant::now().saturating_add(POLL_INTERVAL);
                let now = (class.dtr(), class.rts());
                if now == prev {
                    continue;
                }
                prev = now;
                match now {
                    (false, false) => set_force_download(false),
                    (true, false) => set_force_download(true),
                    (false, true) => enter_bootloader(),
                    (true, true) => {}
                }
            }
            Either::Second(()) => {}
        }
    }
}

/// Copies pending log lines from the ring into the CDC endpoint until the
/// ring is empty or the endpoint errors out. See [`control_and_log`] for the
/// cancellation-safety argument.
async fn flush_logs(class: &mut CdcAcmClass<'static, Driver<'static>>) {
    loop {
        let mut chunk = [0u8; LOG_CHUNK];
        let n = logging::cdc_peek(&mut chunk);
        if n == 0 {
            Timer::after(POLL_INTERVAL).await;
            return;
        }
        let (data, _) = chunk.split_at(n);
        if class.write_packet(data).await.is_err() {
            // The endpoint is disabled (host has not configured us yet) —
            // returning immediately would let `select` complete over and
            // over without ever yielding, starving `device.run()` and
            // stalling enumeration. Wait instead of spinning.
            Timer::after(POLL_INTERVAL).await;
            return;
        }
        logging::cdc_commit(n);
    }
}

/// Set or clear the ROM `FORCE_DOWNLOAD_BOOT` flag in `RTC_CNTL_OPTION1`.
///
/// The register lives in the RTC domain and survives a software reset, so
/// the ROM bootloader sees it on the next boot.
fn set_force_download(armed: bool) {
    // SAFETY: RTC registers are always memory-mapped; `steal()` only bypasses
    // singleton ownership of the peripheral and `modify` leaves every other
    // bit of `RTC_CNTL_OPTION1` untouched.
    let rtc_cntl = unsafe { esp32s2::RTC_CNTL::steal() };
    rtc_cntl
        .options1()
        .modify(|_, w| w.force_download_boot().bit(armed));
}

/// Resets into the ROM USB-CDC download mode (bootloader).
///
/// `(0,1)` is a plain reset: the boot target is decided by the
/// `FORCE_DOWNLOAD_BOOT` flag, which the `(1,0)` state arms. esptool's
/// `--before usb-reset` walk therefore lands in the bootloader. The flag is
/// only cleared again by `inv flash` after flashing (esptool `write-reg`)
/// — the ROM bootloader does not clear it itself.
fn enter_bootloader() -> ! {
    esp_hal::system::software_reset()
}
