---
title: Multi-board HTTP mesh
description: Versioned mesh announcements, applied acknowledgments and heartbeat leases.
---

The current implementation uses `PUT /mesh` protocol v1 on both RP2040 and RP2350.
See [HTTP mesh protocol](../../mesh-protocol.md) for the wire schema, retries,
acknowledgments, session lifecycle, compatibility, and test/deployment guidance.

The web/comms task sends state every 60 seconds and on effect changes. It attempts
one peer per iteration, retries at most three times, and confirms control only
from matching applied acknowledgments. Followers restore their saved local
operation after a 180-second heartbeat lease expires. Effects use IDs rather
than registry indices. FX snapshots and state application stay on the FX task;
atomic effect publication and application tickets connect the two tasks.

Static recipients include `.10`, `.11`, `.12`, `.75`, and `.182`, excluding self.
RP2350 also discovers recipients with mDNS. Mesh acknowledgments replace blocking
ICMP reachability scans. Legacy incoming board-originated `/fx` updates remain
supported with their previous 20-minute timeout; upgrade followers before masters.
