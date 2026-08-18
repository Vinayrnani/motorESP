# motorESP Reverse Tunnel Relay

Bridges public HTTP traffic to an ESP8266 that sits behind CGNAT. The ESP
initiates an outbound TCP connection to this VM; browsers hit the VM's public
HTTP port and the relay pipes the bytes through the ESP's persistent socket.

```
Browser ──► :8280 (public HTTP) ──► relay ──► :9000 (ESP tunnel) ◄── ESP dials OUT
```

## Why a relay at all (not plain socat)

The ESP serves exactly ONE request at a time over its single tunnel stream.
`socat` can only pair sockets 1:1; this relay does HTTP request/response
framing so multiple concurrent browsers queue FIFO onto the one ESP socket
and responses are routed back to the right browser. If the ESP drops, queued
requests fail fast with 502; new requests 502 until the ESP reconnects.

## Quick start (local test)

```bash
node relay/tunnel_relay.js                    # tunnel:9000, http:8280
node relay/test/fake_esp.js                   # pretend-ESP → connects to :9000
curl -i http://localhost:8280/status          # → 200 from fake ESP
curl -i http://localhost:8280/                # fake ESP killed → 502
```

Env overrides: `TUNNEL_PORT` (9000) · `HTTP_PORT` (8280) · `IDLE_MS` (90000).

## Deploy on the VM (systemd)

```bash
sudo mkdir -p /opt/tunnel-relay
sudo cp relay/tunnel_relay.js /opt/tunnel-relay/
sudo cp relay/tunnel-relay.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now tunnel-relay
journalctl -u tunnel-relay -f
```

Firewall (on the VM **and** any cloud security group). This VM uses
`iptables` + `netfilter-persistent` (no ufw). Its INPUT chain ends with a
catch-all `REJECT icmp-host-prohibited`, so ACCEPT rules for both relay
ports are REQUIRED or the ESP's SYN is rejected (`No route to host`):

```bash
sudo iptables -I INPUT 1 -p tcp --dport 9000 -j ACCEPT   # ESP dials in — MUST be publicly reachable
sudo iptables -I INPUT 1 -p tcp --dport 8280 -j ACCEPT   # browsers
sudo netfilter-persistent save                           # persist across reboots
```

NOTE: cloud security-list ingress rules alone are NOT enough — the edge
instance/NAT that owns the public IP must also forward TCP 9000/8280 to
this VM's private IP (10.0.0.63:9000 / 10.0.0.63:8280), and the VM's own
iptables must ACCEPT them. Verify: `curl http://<PUBLIC_IP>:8280/status`
should return the ESP's JSON with `"tunnel":"connected"`.

## Firmware side

In `config.h` on the ESP set:

```c
#define TUNNEL_HOST "68.233.98.190"   // this VM's public IP (numeric only)
#define TUNNEL_PORT 9000
```

Empty `TUNNEL_HOST` = tunnel disabled (LAN-only). The ESP reconnects with
3s→30s backoff, TCP keepalive (30s/10s/3) keeps the CGNAT mapping alive, and
`/status` exposes `"tunnel":"connected|connecting|wait_wifi|disabled"`.

## Behavior notes

- Only ONE ESP tunnel socket is used at a time; a new ESP connection
  replaces the old one.
- Max 8 concurrent browsers (503 beyond), 90s browser idle timeout,
  8 MB request buffer cap.
- Requests are forwarded only when complete (head + Content-Length body);
  responses are matched by queue order, supporting Content-Length, chunked,
  and bodyless (204/304/HEAD) framing.
- Unsolicited ESP bytes (none expected — ESP sends nothing without a
  request) are discarded and logged.