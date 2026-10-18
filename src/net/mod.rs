//! `WiFi` radio and TCP/IP stack (`esp-radio` + `embassy-net`).
//!
//! One of two modes is chosen at boot from [`Config::connection`]:
//!
//! - **Access point** (default): static IPv4 `192.168.4.1/24` plus a DHCP
//!   server handing out leases to AP clients ([`dhcp`]).
//! - **Station**: joins the configured network and gets its address via
//!   DHCP. If the association does not come up within [`STA_CONNECT_TIMEOUT`]
//!   — or the DHCP lease does not arrive within [`STA_DHCP_TIMEOUT`] — the
//!   device switches to AP mode instead so it stays reachable. After a
//!   successful station boot, a lost association is retried every
//!   [`STA_RECONNECT_DELAY`] (the device then stays in station mode, so
//!   reachability depends on the AP coming back).
//!
//! The event loop at the end of [`task`] doubles as the controller
//! keep-alive: dropping `WifiController` de-initializes the radio.
//!
//! In both modes the HTTP/WebSocket server ([`http`], [`ws`]) is started
//! on the stack once the interface is up.

use embassy_executor::Spawner;
use embassy_futures::select::{Either, select};
use embassy_net::{
    Config as NetConfig, Ipv4Address, Ipv4Cidr, Runner, StackResources, StaticConfigV4,
};
use embassy_time::{Duration, Instant, Timer};
use esp_hal::peripherals::WIFI;
use esp_radio::wifi::{
    AuthenticationMethod, Config as WifiConfig, ControllerConfig, Interface, WifiController,
    ap::AccessPointConfig,
    event::{EventInfo, MessageResult},
    sta::StationConfig,
};
use heapless::String;
use static_cell::StaticCell;

use crate::config::{Config, ConnectionType};
use crate::hardware::efuse;

mod dhcp;
pub mod http;
mod ws;

/// Static IPv4 address of the access point — also the DHCP server
/// identifier handed out in option 54.
pub const AP_IP: Ipv4Address = Ipv4Address::new(192, 168, 4, 1);

/// Prefix length of the access-point subnet.
pub const AP_PREFIX: u8 = 24;

/// Maximum time to wait for a station association before falling back to
/// access-point mode.
const STA_CONNECT_TIMEOUT: Duration = Duration::from_secs(15);

/// Maximum time to wait for the station DHCP lease before falling back to
/// access-point mode.
const STA_DHCP_TIMEOUT: Duration = Duration::from_secs(15);

/// Pause between station re-association attempts after a runtime disconnect
/// (the boot-time fallback does not apply here — the device keeps retrying).
const STA_RECONNECT_DELAY: Duration = Duration::from_secs(5);

/// Socket slots in the network stack — four HTTP connection slots plus
/// the DHCP server (AP mode) or DHCP client (station mode), with headroom.
const SOCKETS: usize = 8;

/// Socket storage for the station-mode stack — used when the boot path
/// attempts a station lease (the cell is abandoned if DHCP times out).
static STACK_STA: StaticCell<StackResources<SOCKETS>> = StaticCell::new();

/// Socket storage for the AP-mode stack, kept separate from
/// [`STACK_STA`] so a DHCP-timeout fallback can bring the AP up beside
/// the idle station stack (both cells always cost ~2 KiB of `.bss`).
static STACK_AP: StaticCell<StackResources<SOCKETS>> = StaticCell::new();

/// Resolves the AP SSID: the configured one, or a MAC-derived default.
fn ap_ssid(config: &Config) -> String<32> {
    if config.ap_config.ssid.is_empty() {
        efuse::default_ap_ssid()
    } else {
        config.ap_config.ssid.clone()
    }
}

/// Builds the access-point configuration for `esp-radio`.
///
/// `password` is only applied when it meets the WPA2 minimum length of 8;
/// an empty or shorter password yields an open network.
fn build_ap(ssid: &str, password: &str, wpa2: bool) -> AccessPointConfig {
    let ap = AccessPointConfig::default().with_ssid(ssid);
    if wpa2 {
        ap.with_auth_method(AuthenticationMethod::Wpa2Personal)
            .with_password(password.into())
    } else {
        ap
    }
}

/// Builds the station configuration for `esp-radio`.
///
/// Uses the same WPA2 rule as [`build_ap`]: a password of at least 8
/// characters selects WPA2 personal, anything else joins openly.
fn build_sta(config: &Config) -> StationConfig {
    let password = config.station_config.password.as_str();
    let sta = StationConfig::default().with_ssid(config.station_config.ssid.as_str());
    if password.len() >= 8 {
        sta.with_auth_method(AuthenticationMethod::Wpa2Personal)
            .with_password(password.into())
    } else {
        if !password.is_empty() {
            log::warn!("WiFi station password below the WPA2 minimum of 8 chars — joining openly");
        }
        sta.with_auth_method(AuthenticationMethod::None)
    }
}

/// Deterministic seed for the network stack, derived from the factory
/// MAC address (keeps ephemeral port selection stable across reboots).
fn stack_seed() -> u64 {
    let [b0, b1, b2, b3, b4, b5] = efuse::mac_bytes();
    u64::from_le_bytes([b0, b1, b2, b3, b4, b5, 0, 0])
}

/// Races the station association against [`STA_CONNECT_TIMEOUT`].
///
/// Returns `true` only for a connected result; a disconnect, driver error
/// or timeout all yield `false` so the caller can fall back to AP mode.
async fn try_station(controller: &mut WifiController<'_>) -> bool {
    match select(
        controller.connect_async(),
        Timer::after(STA_CONNECT_TIMEOUT),
    )
    .await
    {
        Either::First(Ok(info)) => {
            log::info!(
                "WiFi station '{}' connected (channel {})",
                info.ssid.as_str(),
                info.channel
            );
            true
        }
        Either::First(Err(err)) => {
            log::warn!("WiFi station connect failed: {err:?}");
            false
        }
        Either::Second(()) => {
            log::warn!("WiFi station connect timed out");
            false
        }
    }
}

/// Brings up the embassy-net stack on the station interface with a DHCP
/// client and waits for the lease.
///
/// Returns `false` when no lease arrives within [`STA_DHCP_TIMEOUT`] so
/// the caller can fall back to AP mode. The station stack is then
/// abandoned: its runner idles safely (the driver refuses TX while the
/// link is down) and never spawned HTTP sockets.
async fn start_station(device: Interface<'static>, spawner: Spawner) -> bool {
    #[allow(clippy::default_trait_access)]
    let net_config = NetConfig::dhcpv4(Default::default());
    let (stack, runner) = embassy_net::new(
        device,
        net_config,
        STACK_STA.init(StackResources::new()),
        stack_seed(),
    );
    spawn_runner(runner, spawner);
    log::info!("WiFi station: waiting for a DHCP lease");
    let deadline = Instant::now().saturating_add(STA_DHCP_TIMEOUT);
    let warn_at = Instant::now().saturating_add(Duration::from_secs(10));
    let mut lease_warned = false;
    loop {
        if let Some(cfg) = stack.config_v4() {
            log::info!("WiFi station up: ip {}", cfg.address.address());
            break;
        }
        if Instant::now() >= deadline {
            log::warn!(
                "WiFi station: no DHCP lease after {} s — falling back to AP",
                STA_DHCP_TIMEOUT.as_secs()
            );
            return false;
        }
        if !lease_warned && Instant::now() >= warn_at {
            log::warn!("WiFi station: no DHCP lease yet — still waiting");
            lease_warned = true;
        }
        Timer::after(Duration::from_millis(250)).await;
    }

    http::spawn_all(spawner, stack);
    log::info!("http/ws server up on port {}", http::HTTP_PORT);
    true
}

/// Brings up the embassy-net stack on the access-point interface with a
/// static address and spawns the DHCP server for AP clients.
async fn start_ap(device: Interface<'static>, ssid: &str, spawner: Spawner) {
    #[allow(clippy::default_trait_access)]
    let net_config = NetConfig::ipv4_static(StaticConfigV4 {
        address: Ipv4Cidr::new(AP_IP, AP_PREFIX),
        gateway: None,
        dns_servers: Default::default(),
    });
    let (stack, runner) = embassy_net::new(
        device,
        net_config,
        STACK_AP.init(StackResources::new()),
        stack_seed(),
    );
    spawn_runner(runner, spawner);
    stack.wait_link_up().await;
    log::info!("WiFi AP up: {AP_IP} (ssid '{ssid}')");

    match dhcp::task(stack) {
        Ok(token) => spawner.spawn(token),
        Err(err) => log::error!("DHCP server spawn failed: {err:?}"),
    }

    http::spawn_all(spawner, stack);
    log::info!("http/ws server up on port {}", http::HTTP_PORT);
}

/// Spawns the embassy-net driver loop (logged, non-fatal on failure).
fn spawn_runner(runner: Runner<'static, Interface<'static>>, spawner: Spawner) {
    match runner_task(runner) {
        Ok(token) => spawner.spawn(token),
        Err(err) => log::error!("network runner spawn failed: {err:?}"),
    }
}

/// Network task: brings up `WiFi` in station or AP mode and runs the
/// event loop.
///
/// Spawned from `main` after logging and the embassy scheduler are up.
/// Never returns — besides logging driver events (client join/leave,
/// station disconnects), the final loop holds the controller alive; in
/// station mode it also re-associates after [`EventInfo::StationDisconnected`].
#[embassy_executor::task]
pub async fn task(wifi: WIFI<'static>, config: Config, spawner: Spawner) {
    let ssid = ap_ssid(&config);
    let password = config.ap_config.password.as_str();
    let wpa2 = password.len() >= 8;
    if !password.is_empty() && !wpa2 {
        log::warn!("WiFi AP password below the WPA2 minimum of 8 chars — open network");
    }

    // Station mode is only attempted with a configured SSID; an empty SSID
    // degrades to the AP so the device never ends up with no radio at all.
    let want_sta =
        config.connection == ConnectionType::WifiSta && !config.station_config.ssid.is_empty();
    if config.connection == ConnectionType::WifiSta && config.station_config.ssid.is_empty() {
        log::warn!("WiFi station mode selected but no SSID configured — starting AP");
    }

    let initial_config = if want_sta {
        log::info!(
            "WiFi: connecting to station '{}'",
            config.station_config.ssid
        );
        WifiConfig::Station(build_sta(&config))
    } else {
        log::info!(
            "WiFi: starting AP '{ssid}' ({})",
            if wpa2 { "WPA2" } else { "open" }
        );
        WifiConfig::AccessPoint(build_ap(ssid.as_str(), password, wpa2))
    };

    let init = esp_radio::wifi::new(
        wifi,
        ControllerConfig::default().with_initial_config(initial_config),
    );
    if let Err(err) = &init {
        log::error!("WiFi init failed: {err:?} — network stack disabled");
    }
    let Ok((mut controller, interfaces)) = init else {
        loop {
            core::future::pending::<()>().await;
        }
    };

    // The chain stops at the first failure — association timeout, DHCP
    // lease timeout or a non-station boot all land in the AP fallback.
    let station_up = want_sta
        && try_station(&mut controller).await
        && start_station(interfaces.station, spawner).await;
    if !station_up {
        if want_sta {
            log::warn!("WiFi station unavailable — falling back to AP '{ssid}'");
            let fallback = controller.set_config(&WifiConfig::AccessPoint(build_ap(
                ssid.as_str(),
                password,
                wpa2,
            )));
            if let Err(err) = fallback {
                log::error!("WiFi AP fallback failed: {err:?} — network stack disabled");
                loop {
                    core::future::pending::<()>().await;
                }
            }
        }
        start_ap(interfaces.access_point, ssid.as_str(), spawner).await;
    }

    // Subscribe after the (mutable) setup above; the subscriber borrow also
    // keeps the controller alive for the loop below. Station mode watches
    // for `StationDisconnected` and re-associates (esp-radio does not
    // reconnect on its own — `connect_async` only covers the first attempt).
    let mut subscriber = match controller.subscribe() {
        Ok(subscriber) => Some(subscriber),
        Err(err) => {
            log::warn!("WiFi event subscription failed: {err:?}");
            None
        }
    };
    loop {
        let link_lost = match subscriber.as_mut() {
            Some(events) => match events.next_event().await {
                MessageResult::Message(info) => {
                    log::info!("WiFi event: {info:?}");
                    station_up && matches!(info, EventInfo::StationDisconnected { .. })
                }
                MessageResult::Lagged(missed) => {
                    log::warn!("WiFi event queue lagged, {missed} dropped");
                    false
                }
            },
            None => core::future::pending::<bool>().await,
        };
        if link_lost {
            log::warn!("WiFi station link lost — re-associating");
            drop(subscriber); // release the &self borrow before &mut controller
            while !try_station(&mut controller).await {
                Timer::after(STA_RECONNECT_DELAY).await;
            }
            subscriber = match controller.subscribe() {
                Ok(subscriber) => Some(subscriber),
                Err(err) => {
                    log::error!("WiFi event subscription failed after reconnect: {err:?}");
                    None
                }
            };
        }
    }
}

/// Drives the network device event loop (spawned by [`spawn_runner`]).
#[embassy_executor::task]
async fn runner_task(mut runner: Runner<'static, Interface<'static>>) {
    runner.run().await;
}
