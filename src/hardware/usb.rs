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

use embassy_futures::join::join;
use embassy_time::{Duration, Timer};
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
use esp_println::println;
use static_cell::StaticCell;

/// Espressif USB vendor ID (same as the ROM bootloader).
const USB_VID: u16 = 0x303A;

/// Espressif generic CDC-ACM product ID.
const USB_PID: u16 = 0x3001;

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
    let class = CdcAcmClass::new(&mut builder, CDC_STATE.init(State::new()), 64);
    let mut device = builder.build();

    println!("USB-CDC ready");
    join(device.run(), control_monitor(&class)).await;
    unreachable!()
}

/// Polls the DTR/RTS control lines and mirrors them onto the strapping
/// semantics (see module docs).
async fn control_monitor(class: &CdcAcmClass<'static, Driver<'static>>) -> ! {
    let mut prev = (class.dtr(), class.rts());
    loop {
        Timer::after(Duration::from_millis(10)).await;
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
