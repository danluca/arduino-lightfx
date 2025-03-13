// Copyright (c) 2025 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef DNSSECTION_H
#define DNSSECTION_H

#include <Arduino.h>
#include <numeric>
#include <array>
#include <algorithm>
#include <LogProxy.h>

typedef enum {
    PacketTypeCompleteRecord,     // All record provide
    PacketTypeCompleteRelease,    // All record release
    PacketTypeAddressRecord,      // A record provide
    PacketTypeAddressRelease,     // A record release
    PacketTypeReverseRecord,      // Reverse mapping provide
    PacketTypeServiceRecord,      // Service record provide (SRV/TXT/PTR)
    PacketTypeServiceRelease,     // Service record release
    PacketTypeProbe,              // Name probe (conflict detection)
    PacketTypeNextSecure,         // NextSecure record (indicate no other records exist)
} PacketType;

typedef struct {
    uint16_t xid;                      // Transaction ID: randomly chosen, used to match responses to queries
    uint8_t recursionDesired : 1;      // RD: Client sets this to request recursive resolution
    uint8_t truncated : 1;             // TC: Set when message is larger than transmission size allows
    uint8_t authoritativeAnswer : 1;     // AA: Server sets this when it's authoritative for the domain
    uint8_t opCode : 4;                // Operation type: 0=Query, 1=IQuery, 2=Status, 4=Notify, 5=Update
    uint8_t queryResponse : 1;         // QR: 0 for queries, 1 for responses
    uint8_t responseCode : 4;          // RCODE: 0=No error, 1=Format error, 2=Server fail, 3=Name error
    uint8_t checkingDisabled : 1;      // CD: Disables DNSSEC validation
    uint8_t authenticatedData : 1;     // AD: Indicates DNSSEC validation passed
    uint8_t zReserved : 1;             // Z: Reserved for future use, must be zero
    uint8_t recursionAvailable : 1;    // RA: Server sets this if it supports recursion
    uint16_t queryCount;               // QDCOUNT: Number of questions in the query section
    uint16_t answerCount;              // ANCOUNT: Number of records in the answer section
    uint16_t authorityCount;           // NSCOUNT: Number of records in the authority section
    uint16_t additionalCount;          // ARCOUNT: Number of records in the additional section
} __attribute__((__packed__)) Header;

// -----------------------------------------------------------------------------------------------

// HEADER

static constexpr uint16_t XID_DEFAULT = 0;

static constexpr uint8_t DNS_BIT_RD = 0;    // Recursion Desired

static constexpr uint8_t DNS_BIT_TC = 1;    // Truncation flag

static constexpr uint8_t DNS_BIT_AA = 2;    // Authoritative Answer
static constexpr uint8_t DNS_AA_NON_AUTHORITATIVE = 0;
static constexpr uint8_t DNS_AA_AUTHORITATIVE = 1;

static constexpr uint8_t DNS_OPCODE_QUERY = 0;     // Standard query
static constexpr uint8_t DNS_OPCODE_IQUERY = 1;    // Inverse query
static constexpr uint8_t DNS_OPCODE_STATUS = 2;    // Server status request
static constexpr uint8_t DNS_OPCODE_NOTIFY = 4;    // Zone change notification
static constexpr uint8_t DNS_OPCODE_UPDATE = 5;    // Dynamic update

static constexpr uint8_t DNS_BIT_QR = 7;    // Query/Response flag
static constexpr uint8_t DNS_QR_QUERY = 0;
static constexpr uint8_t DNS_QR_RESPONSE = 1;

static constexpr uint8_t DNS_RCODE_NOERROR = 0;     // No error
static constexpr uint8_t DNS_RCODE_FORMERR = 1;     // Format error
static constexpr uint8_t DNS_RCODE_SERVFAIL = 2;    // Server failure
static constexpr uint8_t DNS_RCODE_NXDOMAIN = 3;    // Non-existent domain
static constexpr uint8_t DNS_RCODE_NOTIMP = 4;      // Not implemented
static constexpr uint8_t DNS_RCODE_REFUSED = 5;     // Query refused
static constexpr uint8_t DNS_RCODE_YXDOMAIN = 6;    // Name exists when it should not
static constexpr uint8_t DNS_RCODE_YXRRSET = 7;     // RR set exists when it should not
static constexpr uint8_t DNS_RCODE_NXRRSET = 8;     // RR set that should exist does not
static constexpr uint8_t DNS_RCODE_NOTAUTH = 9;     // Server not authoritative
static constexpr uint8_t DNS_RCODE_NOTZONE = 10;    // Name not contained in zone

static constexpr uint8_t DNS_BIT_CD = 4;    // Checking Disabled
static constexpr uint8_t DNS_BIT_AD = 5;    // Authenticated Data
static constexpr uint8_t DNS_BIT_Z = 6;     // Reserved bit
static constexpr uint8_t DNS_BIT_RA = 7;    // Recursion Available

// RR

static constexpr uint8_t DNS_RECORD_HI = 0x00;       // High byte of record type
static constexpr uint8_t DNS_RECORD_A = 0x01;        // IPv4 host address
static constexpr uint8_t DNS_RECORD_NS = 0x02;       // Nameserver
static constexpr uint8_t DNS_RECORD_CNAME = 0x05;    // Canonical name (alias)
static constexpr uint8_t DNS_RECORD_SOA = 0x06;      // Start of Authority
static constexpr uint8_t DNS_RECORD_PTR = 0x0C;      // Domain name pointer
static constexpr uint8_t DNS_RECORD_MX = 0x0F;       // Mail exchange
static constexpr uint8_t DNS_RECORD_TXT = 0x10;      // Text record
static constexpr uint8_t DNS_RECORD_AAAA = 0x1C;     // IPv6 host address
static constexpr uint8_t DNS_RECORD_SRV = 0x21;      // Service location
static constexpr uint8_t DNS_RECORD_OPT = 0x29;      // EDNS options
static constexpr uint8_t DNS_RECORD_NSEC = 0x2F;     // Next Secure record
static constexpr uint8_t DNS_RECORD_ANY = 0xFF;      // Any type (query only)

static constexpr uint8_t DNS_CACHE_FLUSH = 0x80;       // Flag to tell others to flush cached entries
static constexpr uint8_t DNS_CACHE_NO_FLUSH = 0x00;    // Normal caching behavior

static constexpr uint8_t DNS_CLASS_IN = 0x01;    // Internet class

static constexpr uint8_t DNS_COMPRESS_MARK = 0xC0;    // Marker for compressed names

static constexpr uint16_t DNS_TXT_EMPTY_LENGTH = 0x0001;    // Length for empty TXT
static constexpr uint8_t DNS_TXT_EMPTY_CONTENT = 0x00;      // Single null byte

// CONSTANTS

static constexpr size_t DNS_LABEL_LENGTH_MAX = 63;        // Maximum length of a DNS label section
static constexpr size_t DNS_SERVICE_LENGTH_MAX = 100;     // Maximum number of services
static constexpr size_t DNS_PACKET_LENGTH_MAX = 9000;     // Maximum size of DNS packet
static constexpr size_t DNS_PACKET_LENGTH_SAFE = 1410;    // Safe size of DNS packet

static constexpr size_t DNS_RECORD_HEADER_SIZE = 10;    // Type(2) + Class(2) + TTL(4) + Length(2)
static constexpr size_t DNS_SRV_DETAILS_SIZE = 6;       // Priority(2) + Weight(2) + Port(2)

static constexpr uint32_t DNS_PROBE_WAIT_MS = 250;    // Wait time between probes
static constexpr size_t DNS_PROBE_COUNT = 3;          // Number of probes

// ----------------------------------------------------------------------------
class MDNS;

enum class DNSRecordUniqueness {
    Unique,       // A, AAAA, SRV records
    Shared,       // PTR records
    Contextual    // TXT records - unique when with SRV
};

enum class DNSSection {
    Query = 1 << 0,
    Answer = 1 << 1,
    Authority = 1 << 2,
    Additional = 1 << 3,
    All = Query | Answer | Authority | Additional
};
static constexpr DNSSection operator|(const DNSSection a, const DNSSection b) {
    return static_cast<DNSSection>(static_cast<int>(a) | static_cast<int>(b));
}
static constexpr DNSSection operator&(const DNSSection a, const DNSSection b) {
    return static_cast<DNSSection>(static_cast<int>(a) & static_cast<int>(b));
}
static DNSSection getSection(const size_t i, const size_t qd, const size_t an, const size_t ns) {
    if (i < qd) return DNSSection::Query;
    if (i < an) return DNSSection::Answer;
    if (i < ns) return DNSSection::Authority;
    return DNSSection::Additional;
}
__attribute__((unused)) static const char* getSectionName(const DNSSection section) {
    switch (section) {
        case DNSSection::Query: return "query";
        case DNSSection::Answer: return "answer";
        case DNSSection::Authority: return "authority";
        default: return "additional";
    }
}

static constexpr uint8_t calcSupportedRecordTypeByte(const uint8_t type) {
    return (type - 1) / 8;
}
static constexpr uint8_t calcSupportedRecordTypeMask(const uint8_t type) {
    return 1 << (7 - ((type - 1) % 8));
}
static constexpr struct SupportedRecordType {
    uint8_t type;
    uint8_t byte;
    uint8_t mask;
} SupportedRecordTypes[] = {
    { DNS_RECORD_A, calcSupportedRecordTypeByte(DNS_RECORD_A), calcSupportedRecordTypeMask(DNS_RECORD_A) },
    { DNS_RECORD_PTR, calcSupportedRecordTypeByte(DNS_RECORD_PTR), calcSupportedRecordTypeMask(DNS_RECORD_PTR) },
    { DNS_RECORD_TXT, calcSupportedRecordTypeByte(DNS_RECORD_TXT), calcSupportedRecordTypeMask(DNS_RECORD_TXT) },
    { DNS_RECORD_SRV, calcSupportedRecordTypeByte(DNS_RECORD_SRV), calcSupportedRecordTypeMask(DNS_RECORD_SRV) },
    { DNS_RECORD_NSEC, calcSupportedRecordTypeByte(DNS_RECORD_NSEC), calcSupportedRecordTypeMask(DNS_RECORD_NSEC) }
};

struct DNSBitmap {
    static constexpr size_t BITMAP_SIZE = 32;
    static constexpr uint8_t NSEC_WINDOW_BLOCK_0 = 0x00;
    static constexpr uint8_t INITIAL_LENGTH = 2;
    std::array<uint8_t, 2 + BITMAP_SIZE> _data;
    [[nodiscard]] size_t size() const {
        return _data[1];
    }
    [[nodiscard]] const uint8_t* data() const {
        return _data.data();
    }
    DNSBitmap(const std::initializer_list<uint8_t>& types = {}) : _data{} {
        _data[0] = NSEC_WINDOW_BLOCK_0;
        _data[1] = INITIAL_LENGTH;
        for (const auto& type : types)
            addType(type);
    }
    DNSBitmap& addType(const uint8_t type) {
        for (const auto&[rType, byte, mask] : SupportedRecordTypes)
            if (rType == type) {
                const uint8_t offs = 2 + byte;
                _data[offs] |= mask;
                if (_data[1] < (offs + 1)) _data[1] = (offs + 1);
            }
        return *this;
    }
};

struct NameCollector {
    virtual ~NameCollector() = default;
    MDNS& _mdns;
    const Header& _header;
    //
    using LabelOffset = std::pair<String, uint16_t>;
    using Labels = std::vector<LabelOffset>;
    struct Name {
        DNSSection section{};
        Labels labels{};
    };
    using Names = std::vector<Name>;
    Names _names{};
    //
    [[nodiscard]] String uncompress(const size_t target) const {
        for (const auto&[section, labels] : _names)
            for (const auto& [label, offset] : labels)
                if (target >= offset && target < (offset + label.length()))
                    return (target == offset) ? label : label.substring(target - offset);
        log_warn(F("*** WARNING: could not uncompress at %u ***"), target);
        return {};
    }
    [[nodiscard]] String name(const Labels& labels) const {
        return labels.empty() ? String() : std::accumulate(labels.begin(), labels.end(), String(), [](const String& acc, const LabelOffset& label) {
            return acc.isEmpty() ? label.first : acc + "." + label.first;
        });
    }
    //
    [[nodiscard]] String name() const {
        return _names.empty() ? String() : name(_names.back().labels);
    }
    [[nodiscard]] std::vector<String> names(const DNSSection section = DNSSection::All) const {
        std::vector<String> vNames;
        for (const auto&[section, labels] : _names)
            if ((section & section) == section)
                vNames.push_back(name(labels));
        return vNames;
    }
    virtual void begin() {}
    virtual void end() {}
    void process_iscompressed(const uint16_t offs, const DNSSection, const uint16_t current) {
        _names.back().labels.push_back(LabelOffset(uncompress(offs), current));
    }
    void process_nocompressed(const String& label, const DNSSection, const uint16_t current) {
        _names.back().labels.push_back(LabelOffset(label, current));
    }
    void process_begin(const DNSSection section, const uint16_t offset) {
        _names.push_back({ .section = section, .labels = Labels() });
    }
    void process_update(const DNSSection, const uint8_t[4]) {
    }
    void process_end(const DNSSection, const uint16_t) {
    }
    NameCollector(MDNS& mdns, const Header& header) : _mdns(mdns), _header(header){};
};


#endif //DNSSECTION_H
