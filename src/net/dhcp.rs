//! Minimal `DHCPv4` server for access-point clients.
//!
//! Implements the RFC 2131 subset needed to hand out addresses from a
//! fixed pool: `DISCOVER` → `OFFER`, `REQUEST` → `ACK`/`NAK`, plus
//! `DECLINE`, `RELEASE` and `INFORM`. Clients are identified by their
//! hardware address (`chaddr`); every lease slot owns a stable pool
//! address, so a reconnecting device keeps its previous address.
//!
//! Replies are broadcast while the client has no address of its own
//! (sender `0.0.0.0` or the RFC 2131 broadcast flag) and unicast for
//! renewals. All packet access goes through the bounds-checked
//! [`Reader`] and [`Writer`] below — client input is never indexed
//! directly.

use embassy_net::udp::{PacketMetadata, UdpSocket};
use embassy_net::{IpAddress, Ipv4Address, Stack};
use embassy_time::{Duration, Instant};

use super::AP_IP;

/// DHCP server port — clients broadcast their requests here.
const SERVER_PORT: u16 = 67;
/// DHCP client port — every reply is sent there.
const CLIENT_PORT: u16 = 68;

/// First pool address: `<AP_IP network>.<POOL_START>` — assumes the `/24`
/// declared by [`super::AP_PREFIX`].
const POOL_START: u8 = 10;
/// Number of lease slots; slot *i* owns `….<POOL_START + i>` (all slots
/// must fit into that `/24`).
const POOL_SIZE: usize = 8;
/// Lease time handed out to clients, in seconds.
const LEASE_SECS: u64 = 3600;
/// Lease duration (the `embassy-time` spelling of [`LEASE_SECS`]).
const LEASE: Duration = Duration::from_secs(LEASE_SECS);
/// Subnet mask sent with every lease — `/24`, matching
/// [`super::AP_PREFIX`]; the pool layout above only supports a `/24`.
const NETMASK: [u8; 4] = [255, 255, 255, 0];

/// BOOTP `op`: request sent by a client (RFC 951).
const BOOTREQUEST: u8 = 1;
/// BOOTP `op`: reply sent by the server (RFC 951).
const BOOTREPLY: u8 = 2;
/// `htype`: Ethernet hardware type (RFC 826).
const HTYPE_ETHERNET: u8 = 1;
/// `hlen`: six-byte MAC address length.
const HLEN_ETHERNET: u8 = 6;
/// Magic cookie `0x63825363` preceding the options area (RFC 2131 §4.2).
const MAGIC_COOKIE: u32 = 0x6382_5363;
/// RFC 2131 `flags` broadcast bit.
const FLAG_BROADCAST: u16 = 0x8000;

/// Option 0: padding byte (skip).
const OPT_PAD: u8 = 0;
/// Option 1: subnet mask.
const OPT_SUBNET_MASK: u8 = 1;
/// Option 3: default router.
const OPT_ROUTER: u8 = 3;
/// Option 6: DNS server.
const OPT_DNS: u8 = 6;
/// Option 50: requested IP address.
const OPT_REQUESTED_IP: u8 = 50;
/// Option 51: IP address lease time.
const OPT_LEASE_TIME: u8 = 51;
/// Option 53: DHCP message type (see the `MSG_*` constants).
const OPT_MESSAGE_TYPE: u8 = 53;
/// Option 54: server identifier.
const OPT_SERVER_ID: u8 = 54;
/// Option 255: end of the options list.
const OPT_END: u8 = 255;

/// Message type 1: client `DISCOVER`.
const MSG_DISCOVER: u8 = 1;
/// Message type 2: server `OFFER`.
const MSG_OFFER: u8 = 2;
/// Message type 3: client `REQUEST`.
const MSG_REQUEST: u8 = 3;
/// Message type 4: client `DECLINE`.
const MSG_DECLINE: u8 = 4;
/// Message type 5: server `ACK`.
const MSG_ACK: u8 = 5;
/// Message type 6: server `NAK`.
const MSG_NAK: u8 = 6;
/// Message type 7: client `RELEASE`.
const MSG_RELEASE: u8 = 7;
/// Message type 8: client `INFORM`.
const MSG_INFORM: u8 = 8;

/// Bounds-checked cursor over an untrusted packet.
struct Reader<'a> {
    buf: &'a [u8],
}

impl<'a> Reader<'a> {
    /// Wrap a packet slice at position 0.
    fn new(buf: &'a [u8]) -> Self {
        Self { buf }
    }

    /// Consume the next `len` bytes.
    fn take(&mut self, len: usize) -> Option<&'a [u8]> {
        let (head, tail) = self.buf.split_at_checked(len)?;
        self.buf = tail;
        Some(head)
    }

    /// Next byte, `None` once the buffer ends.
    fn u8(&mut self) -> Option<u8> {
        self.take(1)?.first().copied()
    }

    /// Next big-endian `u16`.
    fn u16(&mut self) -> Option<u16> {
        let bytes: [u8; 2] = self.take(2)?.try_into().ok()?;
        Some(u16::from_be_bytes(bytes))
    }

    /// Next big-endian `u32`.
    fn u32(&mut self) -> Option<u32> {
        let bytes: [u8; 4] = self.take(4)?.try_into().ok()?;
        Some(u32::from_be_bytes(bytes))
    }

    /// Next four bytes as an IPv4 address.
    fn ipv4(&mut self) -> Option<Ipv4Address> {
        let [a, b, c, d]: [u8; 4] = self.take(4)?.try_into().ok()?;
        Some(Ipv4Address::new(a, b, c, d))
    }
}

/// Bounds-checked cursor that appends to a reply buffer.
struct Writer<'a> {
    buf: &'a mut [u8],
    pos: usize,
}

impl<'a> Writer<'a> {
    /// Wrap a reply buffer at position 0.
    fn new(buf: &'a mut [u8]) -> Self {
        Self { buf, pos: 0 }
    }

    /// Append raw bytes.
    fn put(&mut self, bytes: &[u8]) -> Option<()> {
        let end = self.pos.checked_add(bytes.len())?;
        self.buf.get_mut(self.pos..end)?.copy_from_slice(bytes);
        self.pos = end;
        Some(())
    }

    /// Append `len` zero bytes.
    fn zeros(&mut self, len: usize) -> Option<()> {
        let end = self.pos.checked_add(len)?;
        self.buf.get_mut(self.pos..end)?.fill(0);
        self.pos = end;
        Some(())
    }

    /// Append one byte.
    fn u8(&mut self, value: u8) -> Option<()> {
        self.put(&[value])
    }

    /// Append a big-endian `u16`.
    fn u16(&mut self, value: u16) -> Option<()> {
        self.put(&value.to_be_bytes())
    }

    /// Append a big-endian `u32`.
    fn u32(&mut self, value: u32) -> Option<()> {
        self.put(&value.to_be_bytes())
    }

    /// Append an IPv4 address.
    fn ipv4(&mut self, value: Ipv4Address) -> Option<()> {
        self.put(&value.octets())
    }

    /// Append a length-prefixed DHCP option.
    fn opt(&mut self, kind: u8, data: &[u8]) -> Option<()> {
        self.u8(kind)?;
        self.u8(u8::try_from(data.len()).ok()?)?;
        self.put(data)
    }
}

/// A validated client message.
struct ClientMsg {
    xid: u32,
    flags: u16,
    mac: [u8; 6],
    ciaddr: Ipv4Address,
    giaddr: Ipv4Address,
    msg_type: u8,
    requested_ip: Option<Ipv4Address>,
    server_id: Option<Ipv4Address>,
}

/// Parse and validate a DHCP request; `None` for anything that is not a
/// well-formed ethernet `BOOTREQUEST` from a client.
fn parse_client(buf: &[u8]) -> Option<ClientMsg> {
    let mut r = Reader::new(buf);
    let op = r.u8()?;
    let htype = r.u8()?;
    let hlen = r.u8()?;
    let _hops = r.u8()?;
    let xid = r.u32()?;
    let _secs = r.u16()?;
    let flags = r.u16()?;
    let ciaddr = r.ipv4()?;
    let _yiaddr = r.ipv4()?;
    let _siaddr = r.ipv4()?;
    let giaddr = r.ipv4()?;
    let hw_addr = r.take(16)?;
    let _sname = r.take(64)?;
    let _file = r.take(128)?;
    let cookie = r.u32()?;
    if op != BOOTREQUEST
        || htype != HTYPE_ETHERNET
        || hlen != HLEN_ETHERNET
        || cookie != MAGIC_COOKIE
    {
        return None;
    }
    let (mac, _padding) = hw_addr.split_at(usize::from(HLEN_ETHERNET));
    let mac = mac.try_into().ok()?;

    let mut msg_type = 0u8;
    let mut requested_ip = None;
    let mut server_id = None;
    while let Some(kind) = r.u8() {
        if kind == OPT_END {
            break;
        }
        if kind == OPT_PAD {
            continue;
        }
        // DHCP options are TLV: type, length, value.
        let len = usize::from(r.u8()?);
        let data = r.take(len)?;
        match kind {
            OPT_MESSAGE_TYPE => {
                if let Some(value) = data.first() {
                    msg_type = *value;
                }
            }
            OPT_REQUESTED_IP => {
                if let Ok([a, b, c, d]) = <[u8; 4]>::try_from(data) {
                    requested_ip = Some(Ipv4Address::new(a, b, c, d));
                }
            }
            OPT_SERVER_ID => {
                if let Ok([a, b, c, d]) = <[u8; 4]>::try_from(data) {
                    server_id = Some(Ipv4Address::new(a, b, c, d));
                }
            }
            _ => {}
        }
    }
    if msg_type == 0 {
        return None;
    }

    Some(ClientMsg {
        xid,
        flags,
        mac,
        ciaddr,
        giaddr,
        msg_type,
        requested_ip,
        server_id,
    })
}

/// One lease slot: a pool address bound to a client hardware address.
#[derive(Clone, Copy)]
struct Lease {
    mac: [u8; 6],
    expires: Instant,
    occupied: bool,
}

/// The server's fixed lease pool.
struct Server {
    leases: [Lease; POOL_SIZE],
}

/// The reply the server decided on for one client message.
struct Action {
    kind: u8,
    yiaddr: Ipv4Address,
    ciaddr: Ipv4Address,
    with_config: bool,
}

impl Server {
    /// Empty pool — every slot free, nothing expired yet.
    const fn new() -> Self {
        Self {
            leases: [Lease {
                mac: [0; 6],
                expires: Instant::from_nanos(0),
                occupied: false,
            }; POOL_SIZE],
        }
    }

    /// Address owned by a pool slot.
    fn pool_addr(slot: usize) -> Option<Ipv4Address> {
        if slot >= POOL_SIZE {
            return None;
        }
        let offset = u8::try_from(slot).ok()?;
        let octet = POOL_START.checked_add(offset)?;
        let [a, b, c, _] = AP_IP.octets();
        Some(Ipv4Address::new(a, b, c, octet))
    }

    /// Slot that owns `ip`, when the address is inside the pool.
    fn slot_of_addr(ip: Ipv4Address) -> Option<usize> {
        let [a, b, c, d] = ip.octets();
        let [ap_a, ap_b, ap_c, _] = AP_IP.octets();
        if [a, b, c] != [ap_a, ap_b, ap_c] {
            return None;
        }
        let slot = d.checked_sub(POOL_START)?;
        if usize::from(slot) < POOL_SIZE {
            Some(usize::from(slot))
        } else {
            None
        }
    }

    /// Slot actively (unexpired) leased to `mac`.
    fn active_slot(&self, mac: [u8; 6], now: Instant) -> Option<usize> {
        self.leases
            .iter()
            .position(|lease| lease.occupied && lease.mac == mac && lease.expires > now)
    }

    /// Extend a lease for another full duration.
    fn refresh(&mut self, slot: usize, now: Instant) {
        if let Some(lease) = self.leases.get_mut(slot) {
            lease.expires = now.saturating_add(LEASE);
        }
    }

    /// Assign a pool address to `mac` (reusing its existing lease, else
    /// claiming a free or expired slot).
    fn assign(&mut self, mac: [u8; 6], now: Instant) -> Option<Ipv4Address> {
        for (i, lease) in self.leases.iter_mut().enumerate() {
            if lease.occupied && lease.mac == mac && lease.expires > now {
                lease.expires = now.saturating_add(LEASE);
                return Self::pool_addr(i);
            }
        }
        for (i, lease) in self.leases.iter_mut().enumerate() {
            if !lease.occupied || lease.expires <= now {
                *lease = Lease {
                    mac,
                    expires: now.saturating_add(LEASE),
                    occupied: true,
                };
                return Self::pool_addr(i);
            }
        }
        None
    }

    /// Bind `mac` to a specific pool address (the `REQUEST` path).
    fn bind_addr(&mut self, mac: [u8; 6], ip: Ipv4Address, now: Instant) -> bool {
        let Some(slot) = Self::slot_of_addr(ip) else {
            return false;
        };
        let Some(lease) = self.leases.get_mut(slot) else {
            return false;
        };
        let active = lease.occupied && lease.expires > now;
        if active && lease.mac != mac {
            return false;
        }
        if active {
            lease.expires = now.saturating_add(LEASE);
        } else {
            *lease = Lease {
                mac,
                expires: now.saturating_add(LEASE),
                occupied: true,
            };
        }
        true
    }

    /// Free a slot (`RELEASE` / `DECLINE`).
    fn free(&mut self, slot: usize) {
        if let Some(lease) = self.leases.get_mut(slot) {
            lease.occupied = false;
        }
    }

    /// Decide the reply for one client message; `None` means no answer.
    fn handle(&mut self, msg: &ClientMsg, now: Instant) -> Option<Action> {
        match msg.msg_type {
            MSG_DISCOVER => {
                let Some(ip) = self.assign(msg.mac, now) else {
                    log::warn!(
                        "DHCP: lease pool exhausted — ignoring DISCOVER from {:02X?}",
                        msg.mac
                    );
                    return None;
                };
                log::info!("DHCP OFFER {ip} for {:02X?}", msg.mac);
                Some(Action {
                    kind: MSG_OFFER,
                    yiaddr: ip,
                    ciaddr: Ipv4Address::UNSPECIFIED,
                    with_config: true,
                })
            }
            MSG_REQUEST => self.handle_request(msg, now),
            MSG_DECLINE => {
                if let Some(ip) = msg.requested_ip
                    && let Some(slot) = Self::slot_of_addr(ip)
                {
                    let owned = self
                        .leases
                        .get(slot)
                        .is_some_and(|lease| lease.occupied && lease.mac == msg.mac);
                    if owned {
                        self.free(slot);
                        log::warn!("DHCP DECLINE: {ip} released, client {:02X?}", msg.mac);
                    } else {
                        log::warn!(
                            "DHCP DECLINE for {ip} ignored — not leased to {:02X?}",
                            msg.mac
                        );
                    }
                }
                None
            }
            MSG_RELEASE => {
                if let Some(sid) = msg.server_id
                    && sid != AP_IP
                {
                    return None;
                }
                if let Some(slot) = self.active_slot(msg.mac, now) {
                    self.free(slot);
                    log::info!("DHCP RELEASE for {:02X?}", msg.mac);
                }
                None
            }
            MSG_INFORM => Some(Action {
                kind: MSG_ACK,
                yiaddr: Ipv4Address::UNSPECIFIED,
                ciaddr: msg.ciaddr,
                with_config: false,
            }),
            _ => None,
        }
    }

    /// `REQUEST`: confirm a lease (`ACK`) or send the client back to
    /// `DISCOVER` (`NAK`).
    fn handle_request(&mut self, msg: &ClientMsg, now: Instant) -> Option<Action> {
        // Offers from another server are none of our business.
        if let Some(sid) = msg.server_id
            && sid != AP_IP
        {
            return None;
        }
        // Pre-configured clients put the address in `requested_ip`,
        // renewing ones in `ciaddr`.
        let requested = if let Some(ip) = msg.requested_ip {
            Some(ip)
        } else if msg.ciaddr.is_unspecified() {
            None
        } else {
            Some(msg.ciaddr)
        };

        let (kind, yiaddr) = match (self.active_slot(msg.mac, now), requested) {
            (Some(slot), Some(req)) => {
                if Self::pool_addr(slot) == Some(req) {
                    self.refresh(slot, now);
                    (MSG_ACK, req)
                } else {
                    (MSG_NAK, Ipv4Address::UNSPECIFIED)
                }
            }
            (Some(_) | None, None) => (MSG_NAK, Ipv4Address::UNSPECIFIED),
            (None, Some(req)) => {
                if self.bind_addr(msg.mac, req, now) {
                    (MSG_ACK, req)
                } else {
                    (MSG_NAK, Ipv4Address::UNSPECIFIED)
                }
            }
        };
        if kind == MSG_NAK {
            log::warn!("DHCP NAK for {:02X?} (requested {requested:?})", msg.mac);
        } else {
            log::info!("DHCP ACK {yiaddr} for {:02X?}", msg.mac);
        }
        Some(Action {
            kind,
            yiaddr,
            ciaddr: Ipv4Address::UNSPECIFIED,
            with_config: kind == MSG_ACK,
        })
    }
}

/// Assemble a reply packet; returns the number of bytes used.
fn build_reply(buf: &mut [u8], msg: &ClientMsg, action: &Action) -> Option<usize> {
    buf.fill(0);
    let mut w = Writer::new(buf);
    w.u8(BOOTREPLY)?;
    w.u8(HTYPE_ETHERNET)?;
    w.u8(HLEN_ETHERNET)?;
    w.u8(0)?;
    w.u32(msg.xid)?;
    w.u16(0)?;
    w.u16(msg.flags)?;
    w.ipv4(action.ciaddr)?;
    w.ipv4(action.yiaddr)?;
    w.ipv4(Ipv4Address::UNSPECIFIED)?;
    w.ipv4(msg.giaddr)?;
    w.put(&msg.mac)?;
    w.zeros(10)?;
    w.zeros(64)?;
    w.zeros(128)?;
    w.u32(MAGIC_COOKIE)?;

    w.opt(OPT_MESSAGE_TYPE, &[action.kind])?;
    w.opt(OPT_SERVER_ID, &AP_IP.octets())?;
    if action.with_config {
        let lease = u32::try_from(LEASE_SECS).ok()?;
        w.opt(OPT_LEASE_TIME, &lease.to_be_bytes())?;
        w.opt(OPT_SUBNET_MASK, &NETMASK)?;
        w.opt(OPT_ROUTER, &AP_IP.octets())?;
        w.opt(OPT_DNS, &AP_IP.octets())?;
    }
    w.u8(OPT_END)?;
    Some(w.pos)
}

/// DHCP server task: answers client requests on the access-point
/// interface until reboot. Spawned by [`super::task`] after link-up.
#[embassy_executor::task]
pub async fn task(stack: Stack<'static>) {
    let mut rx_meta = [PacketMetadata::EMPTY; 4];
    let mut tx_meta = [PacketMetadata::EMPTY; 4];
    let mut rx_ring = [0u8; 1024];
    let mut tx_ring = [0u8; 512];
    let mut socket = UdpSocket::new(
        stack,
        &mut rx_meta,
        &mut rx_ring,
        &mut tx_meta,
        &mut tx_ring,
    );
    if let Err(err) = socket.bind(SERVER_PORT) {
        log::error!("DHCP server: bind({SERVER_PORT}) failed: {err:?}");
        return;
    }
    let [net0, net1, net2, _] = AP_IP.octets();
    let base = Ipv4Address::new(net0, net1, net2, POOL_START);
    log::info!("DHCP server up: {POOL_SIZE} leases from {base}");

    let mut server = Server::new();
    let mut pkt = [0u8; 1024];
    let mut reply = [0u8; 512];
    loop {
        let (len, meta) = match socket.recv_from(&mut pkt).await {
            Ok(received) => received,
            Err(err) => {
                log::warn!("DHCP: receive failed: {err:?}");
                continue;
            }
        };
        // Only IPv4 is enabled, so the pattern is irrefutable.
        let IpAddress::Ipv4(sender) = meta.endpoint.addr;
        log::debug!("DHCP rx {len}B from {sender}:{}", meta.endpoint.port);
        let Some(data) = pkt.get(..len) else {
            continue;
        };
        let Some(msg) = parse_client(data) else {
            log::debug!("DHCP: ignored {len}B from {sender} (parse)");
            continue;
        };
        let Some(action) = server.handle(&msg, Instant::now()) else {
            continue;
        };
        let Some(used) = build_reply(&mut reply, &msg, &action) else {
            continue;
        };
        let Some(packet) = reply.get(..used) else {
            continue;
        };

        // Broadcast until the client owns an address; unicast renewals.
        let dst = if msg.flags & FLAG_BROADCAST == 0 && !sender.is_unspecified() {
            if action.yiaddr.is_unspecified() {
                sender
            } else {
                action.yiaddr
            }
        } else {
            Ipv4Address::BROADCAST
        };
        if let Err(err) = socket.send_to(packet, (dst, CLIENT_PORT)).await {
            log::warn!("DHCP: send to {dst} failed: {err:?}");
        }
    }
}
