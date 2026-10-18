//! Minimal WebSocket server (RFC 6455 subset) with JSON envelopes.
//!
//! Reference infrastructure for the web contributor. Speaks unfragmented
//! text frames only (client frames masked, server frames unmasked) and
//! maps JSON messages onto the config surface:
//!
//! ```json
//! {"type": "config.get", "id": "1"}
//! {"type": "config.set", "id": "2", "config": {"led_brightness": 128}}
//! {"type": "system.get", "id": "3"}
//! {"type": "ping", "id": "4"}
//! {"type": "reset", "id": "5"}
//! ```
//!
//! Replies: `{"type": "ok", "id": …, "config"?| "system"? …}`,
//! `{"type": "pong", "id": …}`, `{"type": "err", "id": …, "code", "message"}`.
//! `config.set` takes a [`ConfigPatch`] — same merge semantics as
//! `POST /api/config`. Fragmented frames, continuations and binary
//! messages end the session without a close frame (extending that to a
//! close code like 1003 is the contributor's call).

use core::fmt::Write as _;

use base64::Engine as _;
use base64::engine::general_purpose::STANDARD as BASE64;
use embassy_net::tcp::TcpSocket;
use embassy_time::{Duration, Instant, Timer, with_timeout};
use embedded_io_async::{Read as _, Write as _};
use heapless::String;
use serde::{Deserialize, Serialize};
use sha1::{Digest, Sha1};

use crate::config::{Config, ConfigPatch};
use crate::storage;

/// RFC 6455 magic GUID for the accept key.
const GUID: &str = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

/// Largest client payload accepted (config patches are well below this).
const MAX_PAYLOAD: usize = 768;

/// Largest server payload emitted (config JSON answers).
const MAX_REPLY: usize = 1024;

/// Per-frame idle budget — dead peers free their slot.
const IDLE_TIMEOUT: Duration = Duration::from_secs(120);

/// Frame opcode: text payload.
const OPCODE_TEXT: u8 = 0x1;
/// Frame opcode: close the connection.
const OPCODE_CLOSE: u8 = 0x8;
/// Frame opcode: ping probe.
const OPCODE_PING: u8 = 0x9;
/// Frame opcode: pong answer.
const OPCODE_PONG: u8 = 0xA;

/// Run one WebSocket session until close; the socket stays borrowed in.
pub(super) async fn serve(socket: &mut TcpSocket<'_>, key: &str) -> Result<(), ()> {
    handshake(socket, key).await?;
    log::info!("ws: session open");

    let mut payload = [0u8; MAX_PAYLOAD];
    loop {
        let frame = with_timeout(IDLE_TIMEOUT, read_frame(socket, &mut payload)).await;
        let (opcode, len) = if let Ok(result) = frame {
            result?
        } else {
            log::debug!("ws: idle timeout");
            return Ok(());
        };
        let data = payload.get(..len).ok_or(())?;
        match opcode {
            OPCODE_TEXT => handle_message(socket, data).await?,
            OPCODE_PING => write_frame(socket, OPCODE_PONG, data).await?,
            OPCODE_PONG => {}
            OPCODE_CLOSE => {
                write_frame(socket, OPCODE_CLOSE, data).await?;
                log::info!("ws: session closed by client");
                return Ok(());
            }
            _ => {
                log::warn!("ws: unsupported opcode {opcode:#x}, closing");
                return Err(());
            }
        }
    }
}

/// Complete the HTTP→WebSocket upgrade handshake.
async fn handshake(socket: &mut TcpSocket<'_>, key: &str) -> Result<(), ()> {
    let mut hasher = Sha1::new();
    hasher.update(key.as_bytes());
    hasher.update(GUID.as_bytes());
    let digest = hasher.finalize();

    let mut raw = [0u8; 28];
    let n = BASE64.encode_slice(digest, &mut raw).map_err(|_| ())?;
    let bytes = raw.get(..n).ok_or(())?;
    let Some(accept) = core::str::from_utf8(bytes)
        .ok()
        .and_then(|text| text.parse::<String<32>>().ok())
    else {
        return Err(());
    };

    let mut response = String::<160>::new();
    write!(
        &mut response,
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: {accept}\r\n\r\n"
    )
    .map_err(|_| ())?;
    socket.write_all(response.as_bytes()).await.map_err(|_| ())
}

/// Read one complete client frame (masked, unfragmented) into `buf`.
async fn read_frame(socket: &mut TcpSocket<'_>, buf: &mut [u8]) -> Result<(u8, usize), ()> {
    let mut head = [0u8; 2];
    socket.read_exact(&mut head).await.map_err(|_| ())?;
    let first = *head.first().ok_or(())?;
    let second = *head.get(1).ok_or(())?;

    if first & 0x70 != 0 || first & 0x80 == 0 {
        // RSV bits set or fragmented — not supported, reject.
        return Err(());
    }
    if second & 0x80 == 0 {
        // Clients must mask (RFC 6455 §5.1).
        return Err(());
    }

    let opcode = first & 0x0f;
    let len = match second & 0x7f {
        0..=125 => usize::from(second & 0x7f),
        126 => {
            let mut ext = [0u8; 2];
            socket.read_exact(&mut ext).await.map_err(|_| ())?;
            usize::from(u16::from_be_bytes(ext))
        }
        _ => return Err(()), // 64-bit lengths unsupported
    };
    if len > buf.len() {
        return Err(());
    }

    let mut mask = [0u8; 4];
    socket.read_exact(&mut mask).await.map_err(|_| ())?;
    let target = buf.get_mut(..len).ok_or(())?;
    socket.read_exact(target).await.map_err(|_| ())?;
    for (byte, mask_byte) in target.iter_mut().zip(mask.iter().cycle()) {
        *byte ^= *mask_byte;
    }
    Ok((opcode, len))
}

/// Write one unmasked server frame.
async fn write_frame(socket: &mut TcpSocket<'_>, opcode: u8, payload: &[u8]) -> Result<(), ()> {
    let mut head = [0u8; 4];
    let first = head.first_mut().ok_or(())?;
    *first = 0x80 | opcode;
    let len = payload.len();
    if len <= 125 {
        let second = head.get_mut(1).ok_or(())?;
        *second = u8::try_from(len).map_err(|_| ())?;
        socket
            .write_all(head.get(..2).ok_or(())?)
            .await
            .map_err(|_| ())?;
    } else {
        let second = head.get_mut(1).ok_or(())?;
        *second = 126;
        let be = u16::try_from(len).map_err(|_| ())?.to_be_bytes();
        head.get_mut(2..4).ok_or(())?.copy_from_slice(&be);
        socket.write_all(&head).await.map_err(|_| ())?;
    }
    socket.write_all(payload).await.map_err(|_| ())
}

/// First-pass envelope: `{"type": "...", "id": "..."}` (extra fields ignored).
///
/// Internally tagged enums need serde's alloc buffering, which `no_std`
/// `serde-json-core` lacks — so requests are parsed twice from the same
/// slice: the head first, then a typed payload for `config.set`.
#[derive(Deserialize)]
struct MessageHead {
    /// Message discriminator.
    #[serde(rename = "type")]
    kind: String<16>,
    /// Correlation id echoed in the reply.
    #[serde(default)]
    id: String<16>,
}

/// Payload of a `config.set` message.
#[derive(Deserialize)]
struct ConfigSetPayload {
    /// Partial config (web: `DeepPartial<Config>`).
    config: ConfigPatch,
}

/// Server reply envelope — flat struct (same JSON as a tagged enum).
#[derive(Serialize)]
struct Reply {
    /// Message discriminator: `ok`, `err`, `pong`.
    #[serde(rename = "type")]
    kind: &'static str,
    /// Correlation id from the request.
    id: String<16>,
    /// Full config (`config.get`).
    #[serde(skip_serializing_if = "Option::is_none")]
    config: Option<Config>,
    /// Runtime info (`system.get`).
    #[serde(skip_serializing_if = "Option::is_none")]
    system: Option<SystemInfo>,
    /// Machine-readable error code (`err`).
    #[serde(skip_serializing_if = "Option::is_none")]
    code: Option<String<24>>,
    /// Human-readable error detail (`err`).
    #[serde(skip_serializing_if = "Option::is_none")]
    message: Option<String<64>>,
}

/// Runtime information payload (`system.get`).
#[derive(Serialize)]
struct SystemInfo {
    /// Milliseconds since boot.
    uptime_ms: u64,
    /// Free internal heap bytes.
    heap_free: usize,
    /// Used internal heap bytes.
    heap_used: usize,
    /// Station/AP MAC address, colon-separated uppercase hex.
    mac: String<18>,
}

impl Reply {
    /// `ok` without payload.
    fn ok(id: String<16>) -> Self {
        Self {
            kind: "ok",
            id,
            config: None,
            system: None,
            code: None,
            message: None,
        }
    }

    /// `ok` carrying the full config.
    fn ok_config(id: String<16>, config: Config) -> Self {
        Self {
            kind: "ok",
            id,
            config: Some(config),
            system: None,
            code: None,
            message: None,
        }
    }

    /// `ok` carrying runtime info.
    fn ok_system(id: String<16>, system: SystemInfo) -> Self {
        Self {
            kind: "ok",
            id,
            config: None,
            system: Some(system),
            code: None,
            message: None,
        }
    }

    /// `pong` liveness reply.
    fn pong(id: String<16>) -> Self {
        Self {
            kind: "pong",
            id,
            config: None,
            system: None,
            code: None,
            message: None,
        }
    }

    /// `err` with code and message.
    fn err(id: String<16>, code: &str, message: &str) -> Self {
        let mut code_buf = String::<24>::new();
        let _ = code_buf.push_str(code);
        let mut message_buf = String::<64>::new();
        let _ = message_buf.push_str(message);
        Self {
            kind: "err",
            id,
            config: None,
            system: None,
            code: Some(code_buf),
            message: Some(message_buf),
        }
    }
}

/// Parse and execute one JSON message, then send the reply.
async fn handle_message(socket: &mut TcpSocket<'_>, data: &[u8]) -> Result<(), ()> {
    let (head, _used): (MessageHead, usize) = match serde_json_core::from_slice(data) {
        Ok(parsed) => parsed,
        Err(err) => {
            log::warn!("ws: bad message: {err}");
            return send_reply(
                socket,
                &Reply::err(String::new(), "bad_request", "unparseable"),
            )
            .await;
        }
    };

    let reply = match head.kind.as_str() {
        "config.get" => Reply::ok_config(head.id, storage::load()),
        "config.set" => match serde_json_core::from_slice::<ConfigSetPayload>(data) {
            Ok((payload, _used)) => {
                let mut config = storage::load();
                if !config.merge_patch(payload.config) {
                    log::warn!("ws: config patch rejected");
                    Reply::err(head.id, "bad_request", "invalid patch")
                } else if storage::save(&config) {
                    config.apply();
                    log::info!("ws: config saved");
                    Reply::ok(head.id)
                } else {
                    log::warn!("ws: config save failed");
                    Reply::err(head.id, "save_failed", "nvs write failed")
                }
            }
            Err(err) => {
                log::warn!("ws: bad config.set payload: {err}");
                Reply::err(head.id, "bad_request", "bad config payload")
            }
        },
        "system.get" => Reply::ok_system(
            head.id,
            SystemInfo {
                uptime_ms: Instant::now().as_millis(),
                heap_free: esp_alloc::HEAP.free(),
                heap_used: esp_alloc::HEAP.used(),
                mac: format_mac(&crate::hardware::efuse::mac_bytes()),
            },
        ),
        "reset" => {
            log::warn!("ws: factory reset requested");
            if storage::clear() {
                let reply = Reply::ok(head.id);
                if send_reply(socket, &reply).await.is_err() {
                    log::warn!("ws: reset reply undeliverable — rebooting anyway");
                }
                Timer::after(Duration::from_millis(100)).await;
                esp_hal::system::software_reset()
            } else {
                log::error!("ws: factory reset failed to clear NVS — not rebooting");
                Reply::err(head.id, "reset_failed", "nvs clear failed")
            }
        }
        "ping" => Reply::pong(head.id),
        other => {
            log::warn!("ws: unknown message type '{other}'");
            Reply::err(head.id, "bad_request", "unknown type")
        }
    };

    send_reply(socket, &reply).await
}

/// Serialize and write one reply frame.
async fn send_reply(socket: &mut TcpSocket<'_>, reply: &Reply) -> Result<(), ()> {
    let mut body = [0u8; MAX_REPLY];
    let n = serde_json_core::to_slice(reply, &mut body).map_err(|_| ())?;
    write_frame(socket, OPCODE_TEXT, body.get(..n).ok_or(())?).await
}

/// Format MAC bytes as `AA:BB:CC:DD:EE:FF`.
fn format_mac(mac: &[u8; 6]) -> String<18> {
    let mut out = String::new();
    for (index, byte) in mac.iter().enumerate() {
        if index > 0 {
            let _ = out.push(':');
        }
        let _ = write!(&mut out, "{byte:02X}");
    }
    out
}
