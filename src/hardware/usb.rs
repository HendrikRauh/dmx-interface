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
//! | 1   | 1   | debounced clear    | clear `FORCE_DOWNLOAD_BOOT`     |
//!
//! esptool's `--before usb-reset` walk is `(1,0)` (arm) -> a ~1-3 ms
//! `(1,1)` blip -> `(0,1)` (reset): the `(0,1)` step boots whichever
//! target `FORCE_DOWNLOAD_BOOT` selects. The `(1,1)` step is the hard
//! case for this state machine, because a terminal program also ends up
//! in `(1,1)`: pyserial asserts DTR and RTS with **separate** ioctls on
//! open, so the firmware sees a `(1,0)` pulse before `(1,1)` — which
//! would leave the flag armed for the lifetime of the connection, and
//! any later reset (panic, `inv monitor`'s own open dance after a device
//! loss) would land in the bootloader instead of the app. So `(1,1)`
//! clears the flag, but only after it persists for [`OPEN_DEBOUNCE`]
//! (20 ms): long enough to outlive a terminal's session-long hold, far
//! longer than the walk's blip — which therefore never disarms the flag
//! mid-walk.
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

/// How long `(1,1)` must persist before it disarms `FORCE_DOWNLOAD_BOOT`.
///
/// pyserial holds `(1,1)` for the whole session (clearing takes effect
/// after this window), while esptool's walk only blips it for ~1-3 ms —
/// shorter than the window, so a flash is never disarmed mid-walk.
const OPEN_DEBOUNCE: Duration = Duration::from_millis(20);

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
/// The flush is gated on DTR: the host's CDC driver (Linux cdc-acm, Windows
/// usbser) binds and submits receive URBs as soon as the device is
/// configured — long before anyone opens the tty. Draining into that sink
/// would swallow boot logs and the panic replay before `inv monitor` gets a
/// chance to read them (the tty layer discards buffered input on open). With
/// the gate, bytes stay in the ring until a host asserts DTR, which every
/// terminal program does on open.
async fn control_and_log(class: &mut CdcAcmClass<'static, Driver<'static>>) -> ! {
    let mut prev = (class.dtr(), class.rts());
    let mut deadline = Instant::now().saturating_add(POLL_INTERVAL);
    let mut open_since: Option<Instant> = None;
    let mut open_cleared = false;
    loop {
        let outcome = select(Timer::at(deadline), flush_logs(class)).await;
        match outcome {
            Either::First(()) => {
                deadline = Instant::now().saturating_add(POLL_INTERVAL);
                let now = (class.dtr(), class.rts());
                if now != prev {
                    prev = now;
                    open_since = None;
                    open_cleared = false;
                    match now {
                        // `(false,false)` idle: disarm immediately. `(true,true)`
                        // starts the debounce instead of clearing (see
                        // [`OPEN_DEBOUNCE`]) — a terminal holds it for the whole
                        // session, esptool's walk only blips it.
                        (false, false) => set_force_download(false),
                        (true, true) => open_since = Some(Instant::now()),
                        // pyserial asserts DTR before RTS: arm on the `(1,0)`
                        // transient, then either the debounce (terminal) or the
                        // `(0,1)` reset (esptool) follows.
                        (true, false) => set_force_download(true),
                        (false, true) => enter_bootloader(),
                    }
                }
                if prev == (true, true)
                    && !open_cleared
                    && open_since.is_some_and(|since| since.elapsed() >= OPEN_DEBOUNCE)
                {
                    set_force_download(false);
                    open_cleared = true;
                }
            }
            Either::Second(()) => {}
        }
    }
}

/// Copies pending log lines from the ring into the CDC endpoint until the
/// ring is empty or the endpoint errors out. See [`control_and_log`] for the
/// cancellation-safety argument and the DTR gate rationale.
async fn flush_logs(class: &mut CdcAcmClass<'static, Driver<'static>>) {
    if !class.dtr() {
        // No terminal attached: keep the ring intact (drop-oldest still
        // bounds it) and idle for one poll interval.
        Timer::after(POLL_INTERVAL).await;
        return;
    }
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
/// `--before usb-reset` walk therefore lands in the bootloader. The ROM
/// bootloader does not clear the flag itself — it is cleared by
/// `inv flash` after flashing (esptool `write-reg`), by `(0,0)`/`(1,1)`
/// line states, or by the terminal-open disarming above.
fn enter_bootloader() -> ! {
    esp_hal::system::software_reset()
}
