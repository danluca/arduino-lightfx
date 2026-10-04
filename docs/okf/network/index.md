# Network

* [Wi-Fi and mDNS](wifi-and-mdns.md) - DHCP Wi-Fi through the on-board CYW43439 and lwIP, connection health checks and reconnects, and mDNS advertising and discovery.
* [REST API](rest-api.md) - Every HTTP endpoint the board serves: status, health, config, tasks, files, firmware and file uploads.
* [Multi-board broadcast](multi-board-broadcast.md) - How a master board pushes effect changes over HTTP to the other boards, found from a static list and mDNS discovery.
* [OTA firmware upgrade](ota-firmware-upgrade.md) - How firmware is uploaded over HTTP, verified with SHA-256, staged in LittleFS and flashed by PicoOTA on reboot.
