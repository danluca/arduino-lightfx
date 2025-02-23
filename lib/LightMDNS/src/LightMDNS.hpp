//  mgream 2024
//  - significantly refactored

//  Copyright (C) 2010 Georg Kaindl
//  http://gkaindl.com
//
//  This file is part of Arduino EthernetBonjour.
//
//  EthernetBonjour is free software: you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public License
//  as published by the Free Software Foundation, either version 3 of
//  the License, or (at your option) any later version.
//
//  EthernetBonjour is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with EthernetBonjour. If not, see
//  <http://www.gnu.org/licenses/>.
//
#pragma once
#ifndef LIGHTMDNS_H
#define LIGHTMDNS_H

#include <Arduino.h>

#include <vector>
#include <set>
#include <LogProxy.h>

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class MDNSTXTBuilder;
class MDNSTXT {

public:
    static constexpr size_t KEY_LENGTH_MAX = 9;        // RFC recommendation
    static constexpr size_t TOTAL_LENGTH_MAX = 255;    // Per TXT string

    struct Entry {
        String key;
        std::vector<uint8_t> value;
        bool binary;
    };
    using Entries = std::vector<Entry>;

    using Builder = MDNSTXTBuilder;

private:
    Entries _entries;
    mutable uint16_t cached_length{ 0 };
    mutable bool length_valid{ false };
    bool validate(const String& key) const;

public:
    bool insert(const String& key, const void* value, size_t length, bool is_binary);

    const Entries& entries() const {
        return _entries;
    }

    size_t size() const {
        return _entries.size();
    }
    uint16_t length() const;
    String toString() const;
};

// -----------------------------------------------------------------------------------------------

class MDNSServiceBuilder;
struct MDNSService {
    using TXT = MDNSTXT;

    struct Config {
        uint16_t priority = 0x0000;
        uint16_t weight = 0x0000;
        // std::vector<String> subtypes;
    };
    enum class Protocol {
        TCP,
        UDP
    };
    static String toString(const Protocol protocol) {
        if (protocol == Protocol::TCP) return "TCP";
        if (protocol == Protocol::UDP) return "UDP";
        return "Unknown";
    }

    uint16_t port;
    Protocol proto;
    String name;
    Config config{};
    TXT text{};
    String _serv{}, _fqsn{};

    using Builder = MDNSServiceBuilder;
};

// -----------------------------------------------------------------------------------------------

class MDNS {

public:
    struct TTLConfig {
        uint32_t announce = 120;     // Default announcement TTL
        uint32_t probe = 0;          // Probe TTL always 0
        uint32_t goodbye = 0;        // Goodbye/release TTL always 0
        uint32_t shared_max = 10;    // Maximum TTL for shared records per RFC
    };

    enum class Status {
        TryLater = 2,
        Success = 1,
        Failure = 0,
        InvalidArgument = -1,
        OutOfMemory = -2,
        ServerError = -3,
        PacketBad = -4,
        NameConflict = -5,
    };
    static String toString(const Status status) {
        switch (status) {
            case Status::TryLater: return "TryLater";
            case Status::Success: return "Success";
            case Status::Failure: return "Failure";
            case Status::InvalidArgument: return "InvalidArgument";
            case Status::OutOfMemory: return "OutOfMemory";
            case Status::ServerError: return "ServerError";
            case Status::PacketBad: return "PacketBad";
            case Status::NameConflict: return "NameConflict";
            default: return "Unknown";
        }
    }

    using Service = MDNSService;
    using Services = std::vector<Service>;
    using ServiceTypes = std::set<String>;

private:
    UDP* _udp;
    IPAddress _addr;
    String _name, _fqhn, _arpa;
    TTLConfig _ttls;
    bool _enabled{ false };

    Status _messageRecv();
    Status _messageSend(uint16_t xid, int type, const Service* service = nullptr);

    unsigned long _announced{ 0 };
    Status _announce();
    Status _conflicted();

    Services _services{};
    ServiceTypes _serviceTypes{};
    void _writeAddressRecord(uint32_t ttl) const;
    void _writeReverseRecord(uint32_t ttl) const;
    void _writeServiceRecord(const Service& service, uint32_t ttl) const;
    void _writeCompleteRecord(uint32_t ttl) const;
    void _writeProbeRecord(uint32_t ttl) const;
    void _writeNextSecureRecord(const String& name, const std::initializer_list<uint8_t>& types, uint32_t ttl, bool includeAdditional = false) const;

    [[nodiscard]] uint32_t _announceTime() const {
        return ((_ttls.announce / 2) + (_ttls.announce / 4)) * static_cast<uint32_t>(1000);
    }

    Status serviceRecordInsert(Service::Protocol proto, uint16_t port, const String& name, const Service::Config& config = Service::Config(), const Service::TXT& text = Service::TXT());    // deprecated
    Status serviceRecordRemove(Service::Protocol proto, uint16_t port, const String& name);                                                                                                  // deprecated
    Status serviceRecordRemove(const String& name);                                                                                                                                                      // deprecated
    Status serviceRecordClear();                                                                                                                                                                     // deprecated

public:
    explicit MDNS(UDP& udp);
    virtual ~MDNS();

    Status begin();
    Status start(const IPAddress& addr, const String& name, bool checkForConflicts = false);
    Status process();
    Status stop();

    Status serviceInsert(const Service& service) {
        return serviceRecordInsert(service.proto, service.port, service.name, service.config, service.text);
    }
    Status serviceRemove(const Service& service) {
        return serviceRecordRemove(service.proto, service.port, service.name);
    }
    Status serviceRemove(const String& name) {
        return name.isEmpty () ? Status::InvalidArgument : serviceRecordRemove(name);
    }
    Status serviceClear() {
        return serviceRecordClear();
    }
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

class MDNSTXTBuilder {
    MDNSTXT _record;
    MDNSTXTBuilder& _add(const String& key, const void* value, const size_t length, const bool is_binary) {
        if (!_record.insert(key, value, length, is_binary))
            log_error("MDNSTXTBuilder::add: TXT insert failed for key: %s, binary %d", key.c_str(), is_binary);
        return *this;
    }

public:
    MDNSTXTBuilder() = default;

    [[nodiscard]] MDNSTXT build() const {
        return _record;
    }
    operator MDNSTXT() const {
        return build();
    }

    MDNSTXTBuilder& add(const String& key) {
        return _add(key, nullptr, 0, false);
    }
    MDNSTXTBuilder& add(const String& key, const String& value) {
        return _add(key, value.c_str(), value.length(), false);
    }
    MDNSTXTBuilder& add(const String& key, const char* value) {
        return add(key, String(value));
    }
    MDNSTXTBuilder& add(const String& key, const bool value) {
        return add(key, String(value ? "true" : "false"));
    }
    MDNSTXTBuilder& add(const String& key, const int value) {
        return add(key, String(value));
    }
    MDNSTXTBuilder& add(const String& key, const uint8_t* value, const size_t length) {
        return _add(key, value, length, true);
    }
};

// -----------------------------------------------------------------------------------------------

class MDNSServiceBuilder {
    using Service = MDNS::Service;

    Service _service{};
    bool _hasName{ false }, _hasPort{ false }, _hasProtocol{ false };
    [[nodiscard]] bool validate() const {
        return (_hasName && _hasPort && _hasProtocol);
    }

public:
    MDNSServiceBuilder() = default;

    [[nodiscard]] MDNS::Service build() const {
        if (!validate())
            log_error("MDNSServiceBuilder::build: invalid service configuration, missing required fields");
        return _service;
    }
    operator MDNS::Service() const {
        return build();
    }

    MDNSServiceBuilder& withName(const String& name) {
        _service.name = name;
        _hasName = true;
        return *this;
    }
    MDNSServiceBuilder& withPort(const uint16_t port) {
        _service.port = port;
        _hasPort = true;
        return *this;
    }
    MDNSServiceBuilder& withProtocol(const Service::Protocol proto) {
        _service.proto = proto;
        _hasProtocol = true;
        return *this;
    }
    MDNSServiceBuilder& withConfig(const Service::Config& config) {
        _service.config = config;
        return *this;
    }
    MDNSServiceBuilder& withPriority(const uint16_t priority) {
        _service.config.priority = priority;
        return *this;
    }
    MDNSServiceBuilder& withWeight(const uint16_t weight) {
        _service.config.weight = weight;
        return *this;
    }
    MDNSServiceBuilder& withTXT(const Service::TXT& txt) {
        _service.text = txt;
        return *this;
    }
};

// -----------------------------------------------------------------------------------------------

#endif    // LIGHTMDNS_H
