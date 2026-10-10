//! Minimal HTTP/1.1 server with fixed connection slots.
//!
//! Reference infrastructure for the web contributor. Serves the embedded
//! single-file UI (`web/dist`), the config REST surface
//! (`GET`/`POST /api/config`) and hands `/ws` upgrades to [`super::ws`].
//!
//! Design: [`SLOTS`] long-lived tasks, each owning one [`TcpSocket`] for
//! its entire life (accept → handle → close → accept). No socket is ever
//! created or dropped after boot — `TcpSocket::new` allocates a slot from
//! the shared pool and re-creating sockets per connection proved fatal on
//! hardware (unlogged reboot). smoltcp accepts one pending connection per
//! listening socket, so [`SLOTS`] tasks accept [`SLOTS`] connections in
//! parallel; SYNs beyond that are RST and the browser retries.

use core::fmt::Write as _;
use core::str;

use embassy_executor::Spawner;
use embassy_net::Stack;
use embassy_net::tcp::{State, TcpSocket};
use embassy_time::{Duration, Instant, Timer, with_timeout};
use embedded_io_async::Write as _;
use heapless::String;
use static_cell::StaticCell;

use crate::config::ConfigPatch;
use crate::storage;

use super::ws;

/// TCP listen port.
pub const HTTP_PORT: u16 = 80;

/// Parallel connection slots. The UI is a single inlined HTML file, so a
/// page load plus font, config fetch and WebSocket stays well below this.
const SLOTS: usize = 4;

/// Per-slot receive buffer (headers plus a config-sized body).
const RX_SIZE: usize = 1536;

/// Per-slot transmit buffer.
const TX_SIZE: usize = 1024;

/// Budget for reading and serving one plain request.
const REQUEST_TIMEOUT: Duration = Duration::from_secs(10);

/// Built UI (run `inv web:build` first — `include_bytes!` tracks the file).
static INDEX_HTML: &[u8] =
    include_bytes!(concat!(env!("CARGO_MANIFEST_DIR"), "/web/dist/index.html"));

/// UI font (`web/src/style.scss` loads `/fonts/Fredoka.ttf`).
static FREDOKA_TTF: &[u8] = include_bytes!(concat!(
    env!("CARGO_MANIFEST_DIR"),
    "/web/dist/fonts/Fredoka.ttf"
));

macro_rules! slot_task {
    ($task_name:ident, $rx_name:ident, $tx_name:ident) => {
        #[embassy_executor::task]
        pub async fn $task_name(stack: Stack<'static>) {
            static $rx_name: StaticCell<[u8; RX_SIZE]> = StaticCell::new();
            static $tx_name: StaticCell<[u8; TX_SIZE]> = StaticCell::new();
            serve(
                stack,
                $rx_name.init([0; RX_SIZE]),
                $tx_name.init([0; TX_SIZE]),
            )
            .await;
        }
    };
}

slot_task!(slot0, SLOT0_RX, SLOT0_TX);
slot_task!(slot1, SLOT1_RX, SLOT1_TX);
slot_task!(slot2, SLOT2_RX, SLOT2_TX);
slot_task!(slot3, SLOT3_RX, SLOT3_TX);

/// Spawn all HTTP slots against the given stack.
///
/// Works in AP and station mode — the stack is the same either way.
pub fn spawn_all(spawner: Spawner, stack: Stack<'static>) {
    macro_rules! spawn_slot {
        ($result:expr) => {
            match $result {
                Ok(token) => spawner.spawn(token),
                Err(err) => log::error!("http: slot task build failed: {err:?}"),
            }
        };
    }
    spawn_slot!(slot0(stack));
    spawn_slot!(slot1(stack));
    spawn_slot!(slot2(stack));
    spawn_slot!(slot3(stack));
}

/// Connection-slot loop: accept → route (or WS session) → recycle, forever.
async fn serve(stack: Stack<'static>, rx: &'static mut [u8], tx: &'static mut [u8]) {
    let mut socket = TcpSocket::new(stack, rx, tx);
    socket.set_timeout(Some(REQUEST_TIMEOUT));
    socket.set_nagle_enabled(false);
    loop {
        if let Err(err) = socket.accept(HTTP_PORT).await {
            log::warn!("http: accept error: {err:?}");
            Timer::after(Duration::from_millis(100)).await;
            continue;
        }
        match with_timeout(REQUEST_TIMEOUT, route(&mut socket)).await {
            Ok(Ok(Outcome::Upgrade(key))) => {
                // The WebSocket session runs outside the request budget;
                // `ws` applies its own per-frame idle timeout.
                socket.set_timeout(None);
                if ws::serve(&mut socket, &key).await.is_err() {
                    log::debug!("http: websocket session ended abnormally");
                }
            }
            Ok(Ok(Outcome::Handled)) => {}
            Ok(Err(())) => log::debug!("http: request failed"),
            Err(_) => log::debug!("http: request timed out"),
        }
        socket.set_timeout(Some(REQUEST_TIMEOUT));
        recycle(&mut socket).await;
    }
}

/// Close gracefully, then wait for `Closed` so `listen()` is legal again.
async fn recycle(socket: &mut TcpSocket<'_>) {
    socket.close();
    let deadline = Instant::now().saturating_add(Duration::from_secs(3));
    while socket.state() != State::Closed && Instant::now() < deadline {
        Timer::after(Duration::from_millis(50)).await;
    }
    if socket.state() != State::Closed {
        socket.abort();
        Timer::after(Duration::from_millis(20)).await;
    }
}

/// What happened to a connection after the request was read.
enum Outcome {
    /// A plain HTTP request was answered.
    Handled,
    /// A WebSocket upgrade was detected; the session key is returned.
    Upgrade(String<32>),
}

/// Why a request could not be read — drives the error response.
enum ReadError {
    /// Transport failure (EOF/reset/timeout); nothing sensible to answer.
    Unreadable,
    /// Headers complete but `Content-Length` is unparseable → answer 400.
    BadLength,
    /// Announced body larger than the request buffer → answer 413.
    TooLarge,
}

/// Read one request and either answer it or flag the upgrade.
async fn route(socket: &mut TcpSocket<'_>) -> Result<Outcome, ()> {
    let mut buf = [0u8; RX_SIZE];
    let n = match read_request(socket, &mut buf).await {
        Ok(n) => n,
        Err(ReadError::Unreadable) => return Err(()),
        Err(ReadError::BadLength) => {
            log::warn!("http: malformed Content-Length");
            send_status(socket, 400).await?;
            return Ok(Outcome::Handled);
        }
        Err(ReadError::TooLarge) => {
            log::warn!("http: payload exceeds request buffer");
            send_status(socket, 413).await?;
            return Ok(Outcome::Handled);
        }
    };
    let head_end = find_headers_end(buf.get(..n).ok_or(())?).ok_or(())?;
    let headers = buf.get(..head_end).ok_or(())?;
    let Ok(parsed) = parse_request_line(headers) else {
        log::warn!("http: malformed request line");
        send_status(socket, 400).await?;
        return Ok(Outcome::Handled);
    };
    let (method, path) = parsed;

    // WebSocket upgrade takes over this connection entirely.
    if method == "GET"
        && path == "/ws"
        && let Some(key) = ws_key(headers)
    {
        return Ok(Outcome::Upgrade(key));
    }

    let Ok(body) = parse_body(buf.get(..n).ok_or(())?, head_end) else {
        log::warn!("http: malformed request body");
        send_status(socket, 400).await?;
        return Ok(Outcome::Handled);
    };
    let status: u16 = match (method, path) {
        ("GET", "/" | "/index.html") => {
            send_response(socket, 200, "text/html; charset=utf-8", INDEX_HTML).await?;
            200
        }
        ("GET", "/fonts/Fredoka.ttf") => {
            send_response(socket, 200, "font/ttf", FREDOKA_TTF).await?;
            200
        }
        ("GET", "/api/config") => {
            get_config(socket).await?;
            200
        }
        ("POST", "/api/config") => post_config(socket, body).await?,
        _ => {
            send_status(socket, 404).await?;
            404
        }
    };
    if path.starts_with("/api/") {
        log::info!("http: {method} {path} -> {status}");
    } else {
        log::debug!("http: {method} {path} -> {status}");
    }
    Ok(Outcome::Handled)
}

/// Read until `\r\n\r\n` plus the announced body; returns total length.
async fn read_request(socket: &mut TcpSocket<'_>, buf: &mut [u8]) -> Result<usize, ReadError> {
    let mut filled = 0usize;
    loop {
        let Some(chunk) = buf.get_mut(filled..) else {
            return Err(ReadError::TooLarge);
        };
        let read = socket
            .read(chunk)
            .await
            .map_err(|_| ReadError::Unreadable)?;
        if read == 0 {
            return Err(ReadError::Unreadable);
        }
        filled = filled.checked_add(read).ok_or(ReadError::Unreadable)?;
        if let Some(head_end) = find_headers_end(buf.get(..filled).ok_or(ReadError::Unreadable)?) {
            let announced = content_length(buf.get(..head_end).ok_or(ReadError::Unreadable)?)
                .map_err(|()| ReadError::BadLength)?;
            let total = head_end.checked_add(announced).ok_or(ReadError::TooLarge)?;
            if total > buf.len() {
                return Err(ReadError::TooLarge);
            }
            if filled >= total {
                return Ok(filled);
            }
        }
    }
}

/// Offset of the `\r\n\r\n` terminator, if present.
fn find_headers_end(buf: &[u8]) -> Option<usize> {
    buf.windows(4).position(|w| w == b"\r\n\r\n")
}

/// Parsed `Content-Length` (0 when absent).
fn content_length(headers: &[u8]) -> Result<usize, ()> {
    /// Lowercase header name searched for (match is case-insensitive).
    const NEEDLE: &[u8] = b"content-length:";
    for line in headers.split(|b| *b == b'\n') {
        let line = trim(line);
        if starts_with_ci(line, NEEDLE) {
            let value = trim(line.get(NEEDLE.len()..).ok_or(())?);
            let text = str::from_utf8(value).map_err(|_| ())?;
            return text.parse().map_err(|_| ());
        }
    }
    Ok(0)
}

/// Method and path of the request line. A block without `\r\n` (header-less
/// minimal request) is treated as the line itself.
fn parse_request_line(headers: &[u8]) -> Result<(&str, &str), ()> {
    let line_end = headers
        .windows(2)
        .position(|w| w == b"\r\n")
        .unwrap_or(headers.len());
    let line = str::from_utf8(headers.get(..line_end).ok_or(())?).map_err(|_| ())?;
    let mut parts = line.split(' ');
    let method = parts.next().ok_or(())?;
    let path = parts.next().ok_or(())?;
    if method.is_empty() || path.is_empty() {
        return Err(());
    }
    Ok((method, path))
}

/// Body slice following the header terminator (clamped to announced length).
fn parse_body(buf: &[u8], head_end: usize) -> Result<&[u8], ()> {
    let start = head_end.checked_add(4).ok_or(())?;
    let announced = content_length(buf.get(..head_end).ok_or(())?)?;
    let end = start.checked_add(announced).ok_or(())?;
    buf.get(start..end).ok_or(())
}

/// Case-insensitive prefix test.
fn starts_with_ci(line: &[u8], prefix: &[u8]) -> bool {
    line.len() >= prefix.len()
        && line
            .get(..prefix.len())
            .is_some_and(|head| head.eq_ignore_ascii_case(prefix))
}

/// Trim ASCII whitespace/CR.
fn trim(mut bytes: &[u8]) -> &[u8] {
    while bytes.first().is_some_and(u8::is_ascii_whitespace) {
        bytes = bytes.get(1..).unwrap_or(bytes);
    }
    while bytes.last().is_some_and(u8::is_ascii_whitespace) {
        bytes = bytes.get(..bytes.len().saturating_sub(1)).unwrap_or(bytes);
    }
    bytes
}

/// Extract `Sec-WebSocket-Key` from the header block.
fn ws_key(headers: &[u8]) -> Option<String<32>> {
    /// Lowercase header name searched for (match is case-insensitive).
    const NEEDLE: &[u8] = b"sec-websocket-key:";
    for line in headers.split(|b| *b == b'\n') {
        let line = trim(line);
        if starts_with_ci(line, NEEDLE) {
            let value = trim(line.get(NEEDLE.len()..)?);
            let key = str::from_utf8(value).ok()?;
            return key.trim().parse().ok();
        }
    }
    None
}

/// `GET /api/config` → full config as JSON.
async fn get_config(socket: &mut TcpSocket<'_>) -> Result<(), ()> {
    let config = storage::load();
    let mut body = [0u8; 768];
    let n = serde_json_core::to_slice(&config, &mut body).map_err(|_| ())?;
    send_response(socket, 200, "application/json", body.get(..n).ok_or(())?).await
}

/// `POST /api/config` → merge patch, persist, apply; returns the status.
async fn post_config(socket: &mut TcpSocket<'_>, body: &[u8]) -> Result<u16, ()> {
    let (patch, _used): (ConfigPatch, usize) = match serde_json_core::from_slice(body) {
        Ok(parsed) => parsed,
        Err(err) => {
            log::warn!("http: bad config patch: {err}");
            send_status(socket, 400).await?;
            return Ok(400);
        }
    };
    let mut config = storage::load();
    if !config.merge_patch(patch) {
        log::warn!("http: config patch rejected");
        send_status(socket, 400).await?;
        return Ok(400);
    }
    if !storage::save(&config) {
        send_status(socket, 500).await?;
        return Ok(500);
    }
    config.apply();
    log::info!("http: config saved via REST");
    send_response(socket, 200, "application/json", b"{}").await?;
    Ok(200)
}

/// Status-only response (e.g. 404).
async fn send_status(socket: &mut TcpSocket<'_>, status: u16) -> Result<(), ()> {
    let reason = match status {
        400 => "Bad Request",
        404 => "Not Found",
        413 => "Payload Too Large",
        500 => "Internal Server Error",
        _ => "OK",
    };
    // 76 bytes worst case ("500 Internal Server Error" head); 404 fits in 64.
    let mut head = String::<96>::new();
    write!(
        &mut head,
        "HTTP/1.1 {status} {reason}\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
    )
    .map_err(|_| ())?;
    socket.write_all(head.as_bytes()).await.map_err(|_| ())
}

/// Status + headers + body; bodies larger than the TX buffer are streamed.
async fn send_response(
    socket: &mut TcpSocket<'_>,
    status: u16,
    content_type: &str,
    body: &[u8],
) -> Result<(), ()> {
    let mut head = String::<128>::new();
    write!(
        &mut head,
        "HTTP/1.1 {status} OK\r\nContent-Type: {content_type}\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
        body.len()
    )
    .map_err(|_| ())?;
    socket.write_all(head.as_bytes()).await.map_err(|_| ())?;
    for chunk in body.chunks(TX_SIZE) {
        socket.write_all(chunk).await.map_err(|_| ())?;
    }
    Ok(())
}
