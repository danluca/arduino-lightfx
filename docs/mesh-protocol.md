# HTTP mesh protocol v1

RP2040 and RP2350 use the same protocol, receiver, and sender state machines.
Their HTTP transport adapters differ because the Nano uses WiFiNINA with
ArduinoHttpClient, while the Plasma uses the framework's HTTPClient.

## Announcements

A master sends `PUT /mesh` immediately when enabled, whenever its selected effect
changes, and every 60 seconds even if the effect stays fixed:

```json
{"protocol":1,"type":"state","master":"board-unique-id","source":"FXPine","session":"boot-session-nonce","sequence":42,"effect":"FXE4"}
```

`master` is the board UID, `source` is its display name, and `session` is a random
64-bit nonce regenerated whenever broadcasting is enabled (including after boot).
`sequence` is a uint32 counter incremented for each announcement. Retries reuse
the original envelope. Effect IDs avoid depending on matching registry order;
unsupported effects receive HTTP 422 instead of selecting a different effect.

State messages are limited to 480 bytes and responses to 512 bytes. Fields are bounded to 64 bytes
for master/source and 32 bytes for session/effect, excluding the terminating NUL.
JSON serialization escapes names, rather than interpolating them into a buffer.

## Acknowledgments and retries

Responses echo `protocol`, `master`, `session`, `sequence`, and `effect`, with
`type: "ack"` and a `status`:

| HTTP | Status | Meaning |
| --- | --- | --- |
| 202 | queued | Accepted into the FX queue; application is pending |
| 200 | applied | FX task applied the requested registry state |
| 400 | invalid | Invalid envelope or unsupported protocol |
| 403 | ignored | Compile-time mesh opt-out |
| 409 | stale / conflict / master-conflict | Old sequence, inconsistent duplicate, or competing master |
| 422 | unknown-effect | Effect ID is absent on this firmware |
| 503 | busy | FX queue full; retry without committing sequence/lease |

Applied means auto advance is disabled and the requested effect has been selected
in the registry; it does not mean the visual transition has finished. The FX task
publishes an atomic application ticket. The web task never waits for that task.
Repeating a queued request checks its ticket and eventually returns `applied`;
repeating an applied request returns the same acknowledgment without another FX
command. Duplicates do not renew the heartbeat lease.

There are at most three transmissions per peer per announcement, at least one
second apart. Timeouts, queued responses, queue-full responses, and invalid or
mismatched acknowledgments retry. Permanent rejection stops that announcement.
The next periodic announcement retries every peer, including peers previously
offline or opted out. One HTTP exchange is performed per web/comms iteration,
allowing inbound requests, other comms work, and watchdog check-ins between peers.
The Nano transport configures 750 ms connection/HTTP timeouts and bounds response
reads, but these are not an overall request deadline: socket close can wait roughly
five seconds and the WiFiNINA SPI readiness wait has no deadline. The exchange
remains synchronous and can delay inbound HTTP and CORE0 health check-ins.
Round-robin scheduling prevents a batch of peer requests in one iteration; it
does not provide nonblocking network I/O. A separate sender task also requires
safe serialization of the shared WiFiNINA command/reply transactions.

The master's `activeClients` includes only peers with a matching applied
acknowledgment in the last three minutes. A follower exposes `controlState`
(`queued` or `applied`), `protocol`, and, once applied, `masterBoard` in status.
ICMP pings are not used as proof of mesh control.

## Session lifecycle

The first accepted announcement saves the local effect and auto-advance setting.
Later announcements keep that snapshot. Unchanged effects do not restart their
animations. Local sleep scheduling remains in force; received effects update the
effect to resume after sleep.

After 180 seconds without a new accepted announcement, restoration is queued.
Queue-full restoration is retried until it can be queued. Restoration re-enables
the saved auto-advance mode and restores the saved effect, respecting sleep.
Changing effect or auto advance locally abandons the snapshot. A subsequent
announcement can start another mesh session using those new local settings.
Enabling master mode first queues restoration of any follower session.

While a session is active, another board UID or source IP cannot replace it.
A new session nonce from the same UID/IP accommodates master restarts. Sequence
comparisons and heartbeat/retry timers handle uint32 rollover. This protocol is
for the existing trusted LAN; its identity fields are not authentication.

## Deployment and mixed versions

Update followers first, then the master. Old masters can continue using the
legacy board-originated `/fx` format; these sessions retain the existing
20-minute inactivity timeout. An active v1 session rejects legacy mesh updates.
New masters use `/mesh` only: old followers return 404 and are not reported as
controlled. Both updated board families interoperate in either role.

The default static list includes the existing Nano addresses `.10`, `.11`, `.12`
and the reserved Plasma addresses `.75`, `.182` on the board's /24 subnet. The
Plasma also retains mDNS discovery for other addresses. Adjust the static list
for a different network.
