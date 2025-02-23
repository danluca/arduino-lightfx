//  mgream 2024

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

#include <cstring>
#include <cstdlib>
#include <Udp.h>
#include <numeric>
#include <array>
#include <algorithm>
#include "Base64.h"
#include "LightMDNS.hpp"

// -----------------------------------------------------------------------------------------------
#define TLD ".local"

static String join(const std::vector<String>& elements, const String& delimiter) {
    return elements.empty() ? String() : std::accumulate(std::next(elements.begin()), elements.end(), elements[0], [&delimiter](const String& a, const String& b) {
        return a + delimiter + b;
    });
}

static constexpr auto SERVICE_SD_fqsn = "_services._dns-sd._udp.local";

// -----------------------------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------------------------

enum class DNSRecordUniqueness {
    Unique,       // A, AAAA, SRV records
    Shared,       // PTR records
    Contextual    // TXT records - unique when with SRV
};

static uint8_t configureCacheFlush(const DNSRecordUniqueness uniqueness, const bool isProbing = false) {
    if (isProbing) return DNS_CACHE_NO_FLUSH;
    return (uniqueness == DNSRecordUniqueness::Unique || uniqueness == DNSRecordUniqueness::Contextual) ? DNS_CACHE_FLUSH : DNS_CACHE_NO_FLUSH;
}
static uint32_t configureTTL(const DNSRecordUniqueness uniqueness, const MDNS::TTLConfig& ttls, const uint32_t ttl) {
    return ttl == 0 ? 0 : (uniqueness == DNSRecordUniqueness::Shared ? std::min(ttl, ttls.shared_max) : ttl);
}

// -----------------------------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------------------------

__attribute__((unused)) static String parseDNSType(const uint16_t type) {
    switch (type) {
        // Standard DNS types
        case 0x0001: return "A";        // IPv4 host address
        case 0x0002: return "NS";       // Authoritative name server
        case 0x0005: return "CNAME";    // Canonical name for an alias
        case 0x0006: return "SOA";      // Start of authority record
        case 0x000C: return "PTR";      // Domain name pointer
        case 0x000D: return "HINFO";    // Host information
        case 0x000F: return "MX";       // Mail exchange
        case 0x0010: return "TXT";      // Text strings
        case 0x001C: return "AAAA";     // IPv6 host address
        case 0x0021:
            return "SRV";    // Service locator
        // EDNS and Security
        case 0x0029: return "OPT";       // EDNS options (RFC 6891)
        case 0x002B: return "DS";        // Delegation signer
        case 0x002E: return "RRSIG";     // DNSSEC signature
        case 0x002F: return "NSEC";      // Next secure record
        case 0x0030: return "DNSKEY";    // DNS public key
        case 0x0032: return "NSEC3";     // NSEC version 3
        case 0x0033:
            return "NSEC3PARAM";    // NSEC3 parameters
        // Modern Extensions
        case 0x0034: return "TLSA";    // TLS cert association
        case 0x0100: return "CAA";     // Cert authority authorization
        case 0x0101:
            return "DHCID";    // DHCP identifier
        // Special Types
        case 0x00F9: return "TKEY";          // Transaction key
        case 0x00FA: return "TSIG";          // Transaction signature
        case 0x00FB: return "DNSKEY_ALT";    // Alternative DNSKEY
        case 0x00FC: return "RRSIG_ALT";     // Alternative RRSIG
        case 0x00FE: return "AXFR";          // Zone transfer
        case 0x00FF:
            return "ANY";    // Match any type
        // Experimental/Local Use (RFC 6762)
        case 0xFF00: return "LLQ";         // Long-lived query
        case 0xFF01: return "ULLQ";        // Update leases
        case 0xFF02: return "PRIVATE1";    // Private use
        case 0xFF03:
            return "PRIVATE2";    // Private use
        // Meta Queries (RFC 6763)
        case 0xFF1F: return "SERVICE_TYPE_ENUM";    // Service type enumeration
        case 0xFF20: return "SERVICE_PORT";         // Service port
        case 0xFF21: return "SERVICE_TXT";          // Service text
        case 0xFF22: return "SERVICE_TARGET";       // Service target host
        default:
            {
                String result = "Unknown(" + String(type, HEX) + ")";
                if (type >= 0xFFF0)
                    result += "/Reserved";
                else if (type >= 0xFF00)
                    result += "/Local";
                return result;
            }
    }
}

__attribute__((unused)) static String parseDNSFlags(const uint8_t flagsByte) {
    if (flagsByte & 0x80) return "FLUSH";
    return String("NO_FLUSH");
}

__attribute__((unused)) static String parseDNSClassOrEDNS(const uint8_t classByte1, const uint8_t classByte2, const uint16_t type) {
    if (type == 0x0029) {    // OPT record
        const uint16_t payloadSize = (static_cast<uint16_t>(classByte1) << 8) | classByte2;
        String result = "UDP_SIZE(" + String(payloadSize) + ")";
        if (payloadSize < 512)
            result += "/Small";
        else if (payloadSize > 1432)
            result += "/Large";
        return result;
    }
    switch (classByte2) {
        case 0x01: return "IN";
        case 0x02: return "CS";
        case 0x03: return "CH";
        case 0x04: return "HS";
        case 0xFE: return "NONE";
        case 0xFF: return "ANY";
        default: return "Unknown(" + String(classByte2, HEX) + ")";
    }
}

__attribute__((unused)) static String parseHeader(const Header& h) {
    static constexpr const char* opcodes[] = { "QUERY", "IQUERY", "STATUS", "RESERVED", "NOTIFY", "UPDATE", "UNK6", "UNK7", "UNK8", "UNK9", "UNK10", "UNK11", "UNK12", "UNK13", "UNK14", "UNK15" };
    static constexpr const char* rcodes[] = { "NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", "NOTIMP", "REFUSED", "YXDOMAIN", "YXRRSET", "NXRRSET", "NOTAUTH", "NOTZONE", "UNK11", "UNK12", "UNK13", "UNK14", "UNK15" };
    return join({ "ID=0x" + String(h.xid, HEX),
                  "QR=" + String(h.queryResponse),
                  "OPCODE=" + String(opcodes[h.opCode]),
                  "AA=" + String(h.authoritativeAnswer),
                  "TC=" + String(h.truncated),
                  "RD=" + String(h.recursionDesired),
                  "RA=" + String(h.recursionAvailable),
                  "Z=" + String(h.zReserved),
                  "AD=" + String(h.authenticatedData),
                  "CD=" + String(h.checkingDisabled),
                  "RCODE=" + String(rcodes[h.responseCode]),
                  "QDCOUNT=" + String(h.queryCount),
                  "ANCOUNT=" + String(h.answerCount),
                  "NSCOUNT=" + String(h.authorityCount),
                  "ARCOUNT=" + String(h.additionalCount) },
                ",");
}

__attribute__((unused)) static String parseControl(const uint8_t ctrl[4]) {
    const uint16_t type = (ctrl[0] << 8) | ctrl[1];
    return parseDNSType(type) + "/" + parseDNSFlags(ctrl[2]) + "/" + parseDNSClassOrEDNS(ctrl[2], ctrl[3], type);    // Pass both bytes
}

__attribute__((unused)) static void parsePacket(const char* label, const uint8_t* data, const size_t size, const size_t offs = 0) {
    static constexpr char lookup[] = "0123456789ABCDEF";

    log_debug(F("    %04X: <%s> : %s"), size, label, parseHeader(*(reinterpret_cast<const Header*>(data))).c_str());
    // should annotate the RHS of the output with some of the details, e.g. using the parse functions above
    for (size_t i = 0; i < size; i += 16) {
        char buffer[(16 * 3 + 2) + 1 + (16 * 1 + 2) + 1];
        char* position = buffer;
        for (size_t j = 0; j < 16; j++) {
            if ((i + j) < size)
                *position++ = lookup[(data[i + j] >> 4) & 0x0F], *position++ = lookup[(data[i + j] >> 0) & 0x0F], *position++ = ' ';
            else
                *position++ = ' ', *position++ = ' ', *position++ = ' ';
            if ((j + 1) % 8 == 0)
                *position++ = ' ';
        }
        *position++ = ' ';
        for (size_t j = 0; j < 16; j++) {
            if ((i + j) < size)
                *position++ = isprint(data[i + j]) ? static_cast<char>(data[i + j]) : '.';
            else
                *position++ = ' ';
            if ((j + 1) % 8 == 0)
                *position++ = ' ';
        }
        *position++ = '\0';
        log_debug(F("    %04X: %s"), offs + i, buffer);
    }
}

// -----------------------------------------------------------------------------------------------

static const IPAddress MDNS_ADDR_MULTICAST(224, 0, 0, 251);
static constexpr uint16_t MDNS_PORT = 5353;

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

static constexpr const char* protocolPostfix(const MDNS::Service::Protocol proto) {
    switch (proto) {
        case MDNS::Service::Protocol::TCP:
            return "._tcp" TLD;
        case MDNS::Service::Protocol::UDP:
            return "._udp" TLD;
    }
    return "";
};

static constexpr bool OPT_DETAILED_CHECKS = true;
static constexpr uint16_t OPT_DETAILED_CHECKS_REASONABLE_COUNT = 100;

static String makeReverseArpaName(const IPAddress& addr) {
    return String(addr[3]) + "." + String(addr[2]) + "." + String(addr[1]) + "." + String(addr[0]) + ".in-addr.arpa";
}

// -----------------------------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------------------------
static bool isValidDNSKeyChar(const char c) {
    return (c >= 0x20 && c <= 0x7E) && c != '=';    // RFC 6763 Section 6.4
}

bool MDNSTXT::validate(const String& key) const {
    if (key.isEmpty() || key.length() > KEY_LENGTH_MAX) return false;
    if (key.charAt(0) == '=') return false;
    return std::all_of(key.begin(), key.end(), isValidDNSKeyChar);
}

bool MDNSTXT::insert(const String& key, const void* value, const size_t length, const bool is_binary) {
    if (!validate(key))
        return false;
    const auto it = std::find_if(_entries.begin(), _entries.end(), [&key](const auto& e) {
        return e.key.equalsIgnoreCase(key);
    });
    if (it != _entries.end()) {
        it->value.assign(static_cast<const uint8_t*>(value), static_cast<const uint8_t*>(value) + length);
        it->binary = is_binary;
    } else
        _entries.push_back({ key, std::vector<uint8_t>(static_cast<const uint8_t*>(value), static_cast<const uint8_t*>(value) + length), is_binary });
    length_valid = false;
    return true;
}
uint16_t MDNSTXT::length() const {
    if (!length_valid) {
        cached_length = std::accumulate(_entries.begin(), _entries.end(), 0U, [](size_t sum, const auto& entry) {
            const size_t value_len = entry.value.empty() ? 0 : (entry.binary ? Base64::length(entry.value.size()) : entry.value.size());
            return sum + 1 + entry.key.length() + (value_len ? value_len + 1 : 0);    // length byte
        });
        length_valid = true;
    }
    return cached_length;
}
String MDNSTXT::toString() const {
    String result;
    for (const auto&[key, value, binary] : _entries) {
        result += (result.isEmpty() ? "" : ",") + key;
        String encoded;
        encoded.reserve(TOTAL_LENGTH_MAX + 1);
        if (!value.empty()) {
            encoded += '=';
            if (binary) {
                std::vector<char> buffer(Base64::length(value.size()) + 1);
                if (Base64::encode(value.data(), value.size(), buffer.data(), buffer.size()))
                    encoded += String(buffer.data());
            } else
                encoded += String(reinterpret_cast<const char*>(value.data()), value.size());
            result += encoded;
        }
    }
    return result;
}

// -----------------------------------------------------------------------------------------------

static size_t sizeofDNSName(const String& name);
static size_t sizeofServiceRecord(const MDNS::Service& service, const String& fqhn);
static size_t sizeofCompleteRecord(const MDNS::Services& services, const MDNS::ServiceTypes& serviceTypes, const String& fqhn);

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifdef DEBUG_MDNS_UDP_READ
static const size_t udp_read_buffer_maximum__global = DNS_PACKET_LENGTH_MAX;
static size_t _udp_read_buffer_length__global = 0;
static uint8_t _udp_read_buffer_content__global[udp_read_buffer_maximum__global];
#define DEBUG_MDNS_UDP_READ_RESET() _udp_read_buffer_length__global = 0
#define DEBUG_MDNS_UDP_READ_DUMP() \
    if (_udp_read_buffer_length__global > 0) parsePacket("UDP_READ", _udp_read_buffer_content__global, _udp_read_buffer_length__global);
#define DEBUG_MDNS_UDP_READ_BYTE(x) \
    if (_udp_read_buffer_length__global < udp_read_buffer_maximum__global) _udp_read_buffer_content__global[_udp_read_buffer_length__global++] = x;
#else
#define DEBUG_MDNS_UDP_READ_RESET()
#define DEBUG_MDNS_UDP_READ_DUMP()
#define DEBUG_MDNS_UDP_READ_BYTE(x)
#endif

static UDP* _udp_read_handler__global = nullptr;
static uint16_t _udp_read_offset__global = 0, _udp_read_length__global = 0;
#define UDP_READ_START() _udp->beginMulticast(MDNS_ADDR_MULTICAST, MDNS_PORT)
#define UDP_READ_STOP() _udp->stop()
#define UDP_READ_BEGIN(u) \
    do { \
        _udp_read_handler__global = u; \
        _udp_read_offset__global = 0; \
        _udp_read_length__global = _udp_read_handler__global->parsePacket(); \
        DEBUG_MDNS_UDP_READ_RESET(); \
    } while (0)
#define UDP_READ_END() \
    do { \
        _udp_read_handler__global->flush(); \
        DEBUG_MDNS_UDP_READ_DUMP(); \
    } while (0)
#define UDP_READ_AVAILABLE() (_udp_read_length__global != static_cast<uint16_t>(0))
#define UDP_READ_BYTE_OR_FAIL(t, x, y) \
    { \
        if (_udp_read_offset__global >= _udp_read_length__global) y; \
        const int _udp_byte = _udp_read_handler__global->read(); \
        if (_udp_byte < 0) y; \
        x = static_cast<t>(_udp_byte); \
        _udp_read_offset__global++; \
        DEBUG_MDNS_UDP_READ_BYTE(static_cast<uint8_t>(_udp_byte)); \
    }
#define UDP_SKIP_BYTE_OR_FAIL(y) \
    { \
        if (_udp_read_offset__global >= _udp_read_length__global) y; \
        const int _udp_byte = _udp_read_handler__global->read(); \
        if (_udp_byte < 0) y; \
        _udp_read_offset__global++; \
        DEBUG_MDNS_UDP_READ_BYTE(static_cast<uint8_t>(_udp_byte)); \
    }
#define UDP_READ_PEEK() _udp_read_handler__global->peek()
#define UDP_READ_LENGTH() _udp_read_length__global
#define UDP_READ_OFFSET() _udp_read_offset__global
#define UDP_READ_PEER_ADDR() _udp_read_handler__global->remoteIP()
#define UDP_READ_PEER_PORT() _udp_read_handler__global->remotePort()

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#ifdef DEBUG_MDNS_UDP_WRITE
static const size_t _udp_write_buffer_maximum__global = DNS_PACKET_LENGTH_MAX;
static size_t _udp_write_buffer_length__global = 0;
static uint8_t _udp_write_buffer_content__global[_udp_write_buffer_maximum__global];
#define DEBUG_MDNS_UDP_WRITE_RESET() _udp_write_buffer_length__global = 0
#define DEBUG_MDNS_UDP_WRITE_DUMP() \
    if (_udp_write_buffer_length__global > 0) parsePacket("UDP_WRITE", _udp_write_buffer_content__global, _udp_write_buffer_length__global);
#define DEBUG_MDNS_UDP_WRITE_BYTE(x) \
    if (_udp_write_buffer_length__global < _udp_write_buffer_maximum__global) _udp_write_buffer_content__global[_udp_write_buffer_length__global++] = x;
#define DEBUG_MDNS_UDP_WRITE_DATA(x, y) \
    for (auto yy = 0; yy < y; yy++) { \
        if (_udp_write_buffer_length__global < _udp_write_buffer_maximum__global) _udp_write_buffer_content__global[_udp_write_buffer_length__global++] = x[yy]; \
    }
#else
#define DEBUG_MDNS_UDP_WRITE_RESET()
#define DEBUG_MDNS_UDP_WRITE_DUMP()
#define DEBUG_MDNS_UDP_WRITE_BYTE(x)
#define DEBUG_MDNS_UDP_WRITE_DATA(x, y)
#endif

static uint16_t _udp_write_offset__global = 0;
#define UDP_WRITE_BEGIN() \
    do { \
        _udp->beginPacket(MDNS_ADDR_MULTICAST, MDNS_PORT); \
        _udp_write_offset__global = 0; \
        DEBUG_MDNS_UDP_WRITE_RESET(); \
    } while (0)
#define UDP_WRITE_END() \
    do { \
        _udp->endPacket(); \
        DEBUG_MDNS_UDP_WRITE_DUMP(); \
    } while (0)
#define UDP_WRITE_BYTE(x) \
    do { \
        _udp->write(x); \
        _udp_write_offset__global++; \
        DEBUG_MDNS_UDP_WRITE_BYTE((x)); \
    } while (0)
#define UDP_WRITE_DATA(x, y) \
    do { \
        _udp->write(x, y); \
        _udp_write_offset__global += (y); \
        DEBUG_MDNS_UDP_WRITE_DATA((x), (y)); \
    } while (0)
#define UDP_WRITE_OFFSET() _udp_write_offset__global

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

// a mess mixed with the specific handlers, this and the Responder need more rework
template<typename Handler> struct UDP_READ_PACKET_CLASS {
    Handler& _handler;
    const Header& _header;

    UDP_READ_PACKET_CLASS(Handler& handler, const Header& header)
        : _handler(handler), _header(header){};
    ~UDP_READ_PACKET_CLASS() {
        UDP_READ_END();
    }

    bool _extractLabels(const DNSSection section, uint16_t* consumed = nullptr) {
        uint8_t size = 0, comp;
        uint16_t used = 0;
        do {
            const uint16_t offset = UDP_READ_OFFSET();
            UDP_READ_BYTE_OR_FAIL(uint8_t, size, break);
            used++;
            if ((size & DNS_COMPRESS_MARK) == DNS_COMPRESS_MARK) {
                UDP_READ_BYTE_OR_FAIL(uint8_t, comp, return false);
                used++;
                const uint16_t offs = ((static_cast<uint16_t>(size) & ~DNS_COMPRESS_MARK) << 8) | static_cast<uint16_t>(comp);
                _handler.process_iscompressed(offs, section, offset);

            } else if (size > 0) {
                String name;
                name.reserve(size + 1);
                for (auto z = 0; z < size; z++) {
                    char c;
                    UDP_READ_BYTE_OR_FAIL(char, c, return false);
                    used++;
                    name += c;
                }
                _handler.process_nocompressed(name, section, offset);
            }
        } while (size > 0 && size <= DNS_LABEL_LENGTH_MAX);
        if (consumed != nullptr) (*consumed) += used;
        return true;
    }
    bool _extractControl(uint8_t control[4]) {
        for (auto z = 0; z < 4; z++)
            UDP_READ_BYTE_OR_FAIL(uint8_t, control[z], return false);
        return true;
    }
    bool _passoverTTL() {
        for (auto i = 0; i < 4; i++)
            UDP_SKIP_BYTE_OR_FAIL(return false);
        return true;
    }
    bool _extractLength(uint16_t* length) {
        uint8_t b1, b2;
        UDP_READ_BYTE_OR_FAIL(uint8_t, b1, return false);
        UDP_READ_BYTE_OR_FAIL(uint8_t, b2, return false);
        (*length) = (static_cast<uint16_t>(b1) << 8) | static_cast<uint16_t>(b2);
        return true;
    }
    bool _passbySRVDetails(uint16_t* consumed = nullptr) {
        for (auto i = 0; i < 6; i++)    // priority, weight, port
            UDP_SKIP_BYTE_OR_FAIL(return false);
        if (consumed) (*consumed) += 6;
        return true;
    }
    bool _passbyMXDetails(uint16_t* consumed = nullptr) {
        for (auto i = 0; i < 2; i++)    // preference
            UDP_SKIP_BYTE_OR_FAIL(return false);
        if (consumed) (*consumed) += 2;
        return true;
    }
    bool _passbySOADetails(uint16_t* consumed = nullptr) {
        for (auto i = 0; i < 20; i++)    // 5 x 32 bit values
            UDP_SKIP_BYTE_OR_FAIL(return false);
        if (consumed) (*consumed) += 20;
        return true;
    }

    bool process() {

        _handler.begin();

        const size_t qd = _header.queryCount, an = qd + _header.answerCount, ns = an + _header.authorityCount, ad = ns + _header.additionalCount;

        for (size_t i = 0; i < ad; i++) {

            const DNSSection section = getSection(i, qd, an, ns);

            log_debug(F("MDNS: packet: %s[%d/%u]: "), getSectionName(section), i + 1, ad);

            _handler.process_begin(section, UDP_READ_OFFSET());

            if (!_extractLabels(section))
                return false;
            uint8_t control[4];
            if (!_extractControl(control))
                return false;
            const uint16_t type = (static_cast<uint16_t>(control[0]) << 8) | static_cast<uint16_t>(control[1]);

            _handler.process_update(section, control);

            const String name = _handler.name();
            log_debug(F("<%s> [%s] (%s)"), name.c_str(), parseControl(control).c_str(), getSectionName(section));

            if (section != DNSSection::Query) {

                if (!_passoverTTL())
                    return false;
                uint16_t length, consumed = 0;
                if (!_extractLength(&length))
                    return false;

                switch (type) {
                    case DNS_RECORD_CNAME:                                               // possible
                    case DNS_RECORD_NS:                                                  // unlikely
                    case DNS_RECORD_PTR:                                                 // typical
                    case DNS_RECORD_NSEC:                                                // typical
                        if (consumed < length && !_extractLabels(section, &consumed))    // target
                            return false;
                        break;
                    case DNS_RECORD_SRV:    // typical
                        if (consumed < length && !_passbySRVDetails(&consumed))
                            return false;
                        if (consumed < length && !_extractLabels(section, &consumed))    // target
                            return false;
                        break;
                    case DNS_RECORD_MX:    // possible
                        if (consumed < length && !_passbyMXDetails(&consumed))
                            return false;
                        if (consumed < length && !_extractLabels(section, &consumed))    // exchanger
                            return false;
                        break;
                    case DNS_RECORD_SOA:                                                 // unlikely
                        if (consumed < length && !_extractLabels(section, &consumed))    // MNAME
                            return false;
                        if (consumed < length && !_extractLabels(section, &consumed))    // RNAME
                            return false;
                        if (consumed < length && !_passbySOADetails(&consumed))
                            return false;
                        break;
                    default: break;
                }

                while (consumed++ < length)
                    UDP_SKIP_BYTE_OR_FAIL(return false);
            }

            if (section != DNSSection::Query && type != DNS_RECORD_OPT && name.isEmpty())
                log_debug(F("\n**** EMPTY ****"));

            _handler.process_end(section, UDP_READ_OFFSET());
        }

        _handler.end();

        return true;
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
    [[nodiscard]] String _uncompress(const size_t target) const {
        for (const auto&[section, labels] : _names)
            for (const auto& [label, offset] : labels)
                if (target >= offset && target < (offset + label.length()))
                    return (target == offset) ? label : label.substring(target - offset);
        log_warn(F("*** WARNING: could not uncompress at %u ***"), target);
        return {};
    }
    [[nodiscard]] String _name(const Labels& labels) const {
        return labels.empty() ? String() : std::accumulate(labels.begin(), labels.end(), String(), [](const String& acc, const LabelOffset& label) {
            return acc.isEmpty() ? label.first : acc + "." + label.first;
        });
    }
    //
    [[nodiscard]] String name() const {
        return _names.empty() ? String() : _name(_names.back().labels);
    }
    [[nodiscard]] std::vector<String> names(const DNSSection section = DNSSection::All) const {
        std::vector<String> names;
        for (const auto&[section, labels] : _names)
            if ((section & section) == section)
                names.push_back(_name(labels));
        return names;
    }
    virtual void begin() {}
    virtual void end() {}
    void process_iscompressed(const uint16_t offs, const DNSSection, const uint16_t current) {
        _names.back().labels.push_back(LabelOffset(_uncompress(offs), current));
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

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

MDNS::MDNS(UDP& udp) : _udp(&udp) {
}
MDNS::~MDNS() {
    stop();
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

MDNS::Status MDNS::begin() {

    log_debug(F("MDNS: begin"));

    return Status::Success;
}

/**
 *
 * @param addr IP address - must be present
 * @param name name associated with the IP address - must be present
 * @param checkForConflicts whether to check for conflicts
 * @return status
 */
MDNS::Status MDNS::start(const IPAddress& addr, const String& name, const bool checkForConflicts) {
    _addr = addr;
    _name = name;
    _fqhn = name + TLD;
    _arpa = makeReverseArpaName(_addr);

    if (!sizeofDNSName(_name)) {
        log_error(F("MDNS: start: failed, invalid name %s"), _name.c_str());
        return Status::InvalidArgument;
    }

    auto status = Status::Success;
    if (!_enabled) {
        if (!UDP_READ_START())
            status = Status::Failure;
        else _enabled = true;
    }

    if (status != Status::Success)
        log_error(F("MDNS: start: failed _udp->beginMulticast error=%s, not active"), toString(status).c_str());
    else {
        log_info(F("MDNS: start: active ip=%s, name=%s"), IPAddress(_addr).toString().c_str(), _fqhn.c_str());
        if (checkForConflicts) {
            for (auto i = 0; i < DNS_PROBE_COUNT; i++) {
                _messageSend(XID_DEFAULT, PacketTypeProbe);
                delay(DNS_PROBE_WAIT_MS);
            }
            delay(DNS_PROBE_WAIT_MS);
        }
        _messageSend(XID_DEFAULT, PacketTypeCompleteRecord);
    }

    return status;
}

MDNS::Status MDNS::stop() {
    if (_enabled) {
        log_info(F("MDNS: stop"));
        // XXX: should send multiple messages 2 seconds apart
        _messageSend(XID_DEFAULT, PacketTypeCompleteRelease);
        UDP_READ_STOP();
        _enabled = false;
    }

    return Status::Success;
}

MDNS::Status MDNS::process() {

    auto status = Status::Success;
    if (_enabled) {
        auto count = 0;
        do {
            count++;
        } while ((status = _messageRecv()) == Status::Success);

        if (status == Status::NameConflict)
            return _conflicted();
        if (status != Status::Success && status != Status::TryLater)
            log_error(F("MDNS: process: failed _messageRecv error=%s"), toString(status).c_str());
        else if ((status = _announce()) != Status::Success)
            log_error(F("MDNS: process: failed _announce error=%s"), toString(status).c_str());
        if (count > 1)
            log_debug(F("MDNS: process [%d]"), count - 1);
    }

    return status;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

MDNS::Status MDNS::serviceRecordInsert(const Service::Protocol proto, const uint16_t port, const String& name, const Service::Config& config, const Service::TXT& text) {

    log_debug(F("MDNS: serviceRecordInsert: proto=%s, port=%u, name=%s, text.length=%d,text=[%s]"), Service::toString(proto).c_str(), port, name.c_str(), text.length(), text.toString().c_str());

    if (name.isEmpty() || port == 0 || (proto != Service::Protocol::TCP && proto != Service::Protocol::UDP))
        return Status::InvalidArgument;
    if (_services.size() >= DNS_SERVICE_LENGTH_MAX)
        return Status::InvalidArgument;
    if (!sizeofDNSName(name))
        return Status::InvalidArgument;
    if (std::any_of(text.entries().begin(), text.entries().end(), [](const auto& it) {
            return it.key.length() > Service::TXT::TOTAL_LENGTH_MAX;
        }))
        return Status::InvalidArgument;

    Service serviceNew{ .port = port, .proto = proto, .name = name, .config = config, .text = text, ._serv = name.substring(name.lastIndexOf('.') + 1) + protocolPostfix(proto), ._fqsn = name + protocolPostfix(proto) };

    if ((sizeof(Header) + sizeofCompleteRecord(_services, _serviceTypes, _fqhn) + sizeofServiceRecord(serviceNew, _fqhn)) > DNS_PACKET_LENGTH_SAFE)    // could solve with truncation support
        return Status::OutOfMemory;

    const auto& service = _services.emplace_back(serviceNew);
    _serviceTypes.insert(service._serv);
    if (_enabled)
        _messageSend(XID_DEFAULT, PacketTypeServiceRecord, &service);
    return Status::Success;
}

MDNS::Status MDNS::serviceRecordRemove(const Service::Protocol proto, const uint16_t port, const String& name) {

    log_debug(F("MDNS: serviceRecordRemove: proto=%s, port=%u, name=%s"), Service::toString(proto).c_str(), port, name.c_str());

    size_t count = 0;
    _serviceTypes.clear();
    _services.erase(std::remove_if(_services.begin(), _services.end(), [&](const Service& service) {
        if (!(service.port == port && service.proto == proto && (name.isEmpty() || service.name == name))) {
            _serviceTypes.insert(service._serv);
            return false;
        }
        if (_enabled)
            _messageSend(XID_DEFAULT, PacketTypeServiceRelease, &service);
        count++;
        return true;
    }), _services.end());

    return count == 0 ? Status::InvalidArgument : Status::Success;
}

MDNS::Status MDNS::serviceRecordRemove(const String& name) {

    log_debug(F("MDNS: serviceRecordRemove: name=%s"), name.c_str());

    size_t count = 0;
    _serviceTypes.clear();
    _services.erase(std::remove_if(_services.begin(), _services.end(), [&](const Service& service) {
        if (service.name != name) {
            _serviceTypes.insert(service._serv);
            return false;
        }
        if (_enabled)
            _messageSend(XID_DEFAULT, PacketTypeServiceRelease, &service);
        count++;
        return true;
    }), _services.end());

    return count == 0 ? Status::InvalidArgument : Status::Success;
}

MDNS::Status MDNS::serviceRecordClear() {

    log_debug(F("MDNS: serviceRecordClear"));

    if (_enabled)
        for (const auto& service : _services)
            _messageSend(XID_DEFAULT, PacketTypeServiceRelease, &service);
    _services.clear();
    _serviceTypes.clear();

    return Status::Success;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

MDNS::Status MDNS::_announce() {

    if (_enabled && (millis() - _announced) > _announceTime()) {

        log_debug(F("MDNS: announce: services (%d)"), _services.size());

        _messageSend(XID_DEFAULT, PacketTypeCompleteRecord);

        _announced = millis();
    }
    return Status::Success;
}

MDNS::Status MDNS::_conflicted() {

    log_debug(F("MDNS: conflicted: name=%s (will stop)"), _name.c_str());

    stop();
    return Status::NameConflict;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

static const char* checkHeader(const Header& header, const uint16_t packetSize, const int firstByte) {
    if (packetSize < (sizeof(Header) + (header.queryCount * 6) + (header.authorityCount * 6)))
        return "packet too small for claimed record counts";
    if (header.opCode > DNS_OPCODE_UPDATE)
        return "invalid opcode";
    if (header.responseCode > DNS_RCODE_NOTZONE)
        return "invalid response code";
    if (header.queryResponse == 0 && header.authoritativeAnswer == 1)
        return "query with AA set";
    if (header.queryCount > OPT_DETAILED_CHECKS_REASONABLE_COUNT || header.answerCount > OPT_DETAILED_CHECKS_REASONABLE_COUNT || header.authorityCount > OPT_DETAILED_CHECKS_REASONABLE_COUNT || header.additionalCount > OPT_DETAILED_CHECKS_REASONABLE_COUNT)
        return "unreasonable record counts";
    if (header.zReserved != 0)
        return "reserved bit set";
    if (firstByte < 0 || firstByte > DNS_LABEL_LENGTH_MAX)
        return "invalid first label length";
    if (header.truncated && packetSize < 512)
        return "suspicious: TC set but packet small";
    return nullptr;
}

static const char* checkAddress(const IPAddress& addrLocal, const IPAddress& addr) {
    if (addr[0] == 0 && (addr[1] | addr[2] | addr[3]) == 0)
        return "invalid unspecified address (0.0.0.0)";
    if (addr[0] == 127)
        return "invalid loopback address (127.x.x.x)";
    if (addr[0] == 169 && addr[1] == 254) {    // link-local
        if (addr[2] == 0 || addr[2] == 255)
            return "invalid link-local broadcast (169.254.0|255.x)";
        if (!(addrLocal[0] == 169 && addrLocal[1] == 254 && addr[2] == addrLocal[2]))
            return "invalid link-local subnet mismatch";
    }
    return nullptr;
}

// -----------------------------------------------------------------------------------------------
static MDNS::Status packetFailedHandler(const Header& header, const char* error) {
    log_debug(F("MDNS: packet: faulty(%s), %s / %s:%u"), error, parseHeader(header).c_str(), UDP_READ_PEER_ADDR().toString().c_str(), UDP_READ_PEER_PORT());
    UDP_READ_END();
    return MDNS::Status::PacketBad;
}

MDNS::Status MDNS::_messageRecv() {
    const char* detailedError = nullptr;

    UDP_READ_BEGIN(_udp);
    if (!UDP_READ_AVAILABLE())
        return Status::TryLater;

    log_debug(F("MDNS: packet: receiving, size=%u"), UDP_READ_LENGTH());

    Header header;
    for (auto z = 0; z < sizeof(Header); z++)
        UDP_READ_BYTE_OR_FAIL(uint8_t, reinterpret_cast<uint8_t*>(&header)[z], return packetFailedHandler(header, "invalid header"));    // should throw
    header.xid = ntohs(header.xid);
    header.queryCount = ntohs(header.queryCount);
    header.answerCount = ntohs(header.answerCount);
    header.authorityCount = ntohs(header.authorityCount);
    header.additionalCount = ntohs(header.additionalCount);

    if ((detailedError = checkAddress(_addr, UDP_READ_PEER_ADDR())) != nullptr)
        return packetFailedHandler(header, detailedError);
    if (OPT_DETAILED_CHECKS && (detailedError = checkHeader(header, UDP_READ_LENGTH(), UDP_READ_PEEK())) != nullptr)
        return packetFailedHandler(header, detailedError);
    if (header.truncated)
        log_debug(F("MDNS: packet: received truncated from %s, but will proceed"), UDP_READ_PEER_ADDR().toString().c_str());

    if ((header.authorityCount > 0 || header.queryResponse == DNS_QR_RESPONSE) && UDP_READ_PEER_PORT() == MDNS_PORT) {
        log_debug(F("MDNS: packet: checking, %s / %s:%u"), parseHeader(header).c_str(), UDP_READ_PEER_ADDR().toString().c_str(), UDP_READ_PEER_PORT());
        NameCollector collector(*this, header);
        if (UDP_READ_PACKET_CLASS processor(collector, header); !processor.process())
            return Status::PacketBad;    // should throw
        for (const auto& name : collector.names(DNSSection::Answer | DNSSection::Authority | DNSSection::Additional)) {
            if (name.equalsIgnoreCase(_fqhn))    // XXX should check against services
                if ((header.authorityCount > 0 && UDP_READ_PEER_ADDR() != _addr) || (header.authorityCount == 0 && header.queryResponse == DNS_QR_RESPONSE)) {
                    log_debug(F("MDNS: conflict detected in probe: %s from %s"), _fqhn.c_str(), UDP_READ_PEER_ADDR().toString().c_str());
                    return Status::NameConflict;    // should throw
                }
        }
    } else if (header.queryResponse == DNS_QR_QUERY && header.opCode == DNS_OPCODE_QUERY && UDP_READ_PEER_PORT() == MDNS_PORT) {
        log_debug(F("MDNS: packet: processing, %s / %s:%u"), parseHeader(header).c_str(), UDP_READ_PEER_ADDR().toString().c_str(), UDP_READ_PEER_PORT());
        /////////////////////////////////////////////
        /////////////////////////////////////////////
        struct Responder {
            MDNS& _mdns;
            const Header& _header;
            //
            // this is all horrible and brittle and needs replacement, but is getting there ...
            const size_t recordsLengthStatic, recordsLength;
            struct _matcher_t {
                const char* name{};
                int length{};
                int match = 1;
                uint16_t position = 0;
                bool requested = false, unsupported = false;
            };
            std::vector<_matcher_t> recordsMatcherTop{}, recordsMatcherEach{};
            uint16_t _starting{};
            uint8_t _control[4]{};
            //
            int matchStringPart(const char** pCmpStr, int* pCmpLen, const uint8_t* data, const int dataLen) {
                const auto _memcmp_caseinsensitive = [](const char* a, const unsigned char* b, const int l) -> int {
                    for (auto i = 0; i < l; i++) {
                        if (tolower(a[i]) < tolower(b[i])) return -1;
                        if (tolower(a[i]) > tolower(b[i])) return 1;
                    }
                    return 0;
                };
                const int matches = (*pCmpLen >= dataLen) ? 1 & (_memcmp_caseinsensitive(*pCmpStr, data, dataLen) == 0) : 0;
                *pCmpStr += dataLen;
                *pCmpLen -= dataLen;
                if ('.' == **pCmpStr)
                    (*pCmpStr)++, (*pCmpLen)--;
                return matches;
            };
            //

            [[nodiscard]] String name() const {
                return "UNSUPPORTED";
            }
            void process_iscompressed(const uint16_t offs, const DNSSection section, const uint16_t) {
                if (section != DNSSection::Query) return;
                log_debug(F("(%04X)"), offs);
                for (auto& m : recordsMatcherEach)
                    if (m.position && m.position != offs)
                        m.match = 0;
            };
            void process_nocompressed(const String& name, const DNSSection section, const uint16_t) {
                if (section != DNSSection::Query) return;
                log_debug(F("[%s]"), name.c_str());
                for (auto& m : recordsMatcherEach)
                    if (!m.requested && m.match)
                        m.match &= matchStringPart(&m.name, &m.length, reinterpret_cast<const uint8_t*>(name.c_str()), static_cast<int>(name.length()));
            };
            void process_begin(const DNSSection section, const uint16_t starting) {
                if (section != DNSSection::Query) return;
                _starting = starting;
            }
            void process_update(const DNSSection section, const uint8_t control[4]) {
                if (section != DNSSection::Query) return;
                memcpy(_control, control, sizeof(_control));
            }
            void process_end(const DNSSection section, const uint16_t) {
                if (section != DNSSection::Query) return;
                size_t r = 0;
                for (auto& m : recordsMatcherEach) {
                    if (!m.requested && m.match && !m.length) {
                        if (!m.position)
                            m.position = _starting;
                        if (_control[0] == DNS_RECORD_HI && (_control[2] == DNS_CACHE_NO_FLUSH || _control[2] == DNS_CACHE_FLUSH) && _control[3] == DNS_CLASS_IN) {
                            if (r == 0) {    // Query for our hostname
                                if (_control[1] == DNS_RECORD_A)
                                    m.requested = true;
                                else
                                    m.unsupported = true;
                            } else if (r == 1) {    // Query for our address
                                if (_control[1] == DNS_RECORD_PTR)
                                    m.requested = true;
                                else
                                    m.unsupported = true;
                            } else {    // Query for our service
                                if (_control[1] == DNS_RECORD_PTR || _control[1] == DNS_RECORD_TXT || _control[1] == DNS_RECORD_SRV)
                                    m.requested = true;
                                else
                                    m.unsupported = true;
                            }
                        }
                    }
                    recordsMatcherTop[r].requested = m.requested;
                    recordsMatcherTop[r].unsupported = m.unsupported;
                    r++;
                }
                recordsMatcherEach = recordsMatcherTop;
            };

            void begin() {
                size_t j = 0;
                // XXX should build once and cache ... and update each time service name / etc is changed
                recordsMatcherTop[j].name = _mdns._fqhn.c_str(), recordsMatcherTop[j].length = _mdns._fqhn.length(), j++;
                recordsMatcherTop[j].name = _mdns._arpa.c_str(), recordsMatcherTop[j].length = _mdns._arpa.length(), j++;
                recordsMatcherTop[j].name = SERVICE_SD_fqsn, recordsMatcherTop[j].length = strlen(SERVICE_SD_fqsn), j++;
                for (const auto& r : _mdns._services)    // XXX should only include unique r._serv ...
                    recordsMatcherTop[j].name = r._serv.c_str(), recordsMatcherTop[j].length = r._serv.length(), j++;
                    //
#ifdef DEBUG_MDNS
                for (const auto& m : recordsMatcherTop)
                    log_debug(F("MDNS: packet: processing, matching[]: <%s>: %d/%d/%d"), m.name, m.match, m.length, m.position);
#endif
                recordsMatcherEach = recordsMatcherTop;
            };
            void end() {
                // XXX should coaescle into single response(s)
                // XXX should only have unique service names and match from that
                if (recordsMatcherTop[0].unsupported || recordsMatcherTop[1].unsupported || recordsMatcherTop[2].unsupported) {
                    log_debug(F("MDNS: packet: processing, negated[%d/%d/%d]"), recordsMatcherTop[0].unsupported, recordsMatcherTop[1].unsupported, recordsMatcherTop[2].unsupported);
                    _mdns._messageSend(_header.xid, PacketTypeNextSecure);
                }
                if (recordsMatcherTop[0].requested) {
                    log_debug(F("MDNS: packet: processing, matched[NAME]: %s"), recordsMatcherTop[0].name);
                    _mdns._messageSend(_header.xid, PacketTypeAddressRecord);
                }
                if (recordsMatcherTop[1].requested) {
                    log_debug(F("MDNS: packet: processing, matched[ADDR]: %s"), recordsMatcherTop[1].name);
                    _mdns._messageSend(_header.xid, PacketTypeAddressRecord);
                }
                if (recordsMatcherTop[2].requested) {
                    log_debug(F("MDNS: packet: processing, matched[DISC]: %s"), recordsMatcherTop[2].name);
                    _mdns._messageSend(_header.xid, PacketTypeCompleteRecord);
                } else {
                    size_t mi = 0;
                    for (const auto& r : _mdns._services) {
                        const auto& m = recordsMatcherTop[mi + recordsLengthStatic];
                        if (m.requested) {
                            log_debug(F("MDNS: packet: processing, matched[SERV:%d]: %s"), mi, m.name);
                            _mdns._messageSend(_header.xid, PacketTypeServiceRecord, &r);
                        }
                        if (m.unsupported) {
                            log_debug(F("MDNS: packet: processing, negated[SERV:%d]: %s"), mi, m.name);
                            _mdns._messageSend(_header.xid, PacketTypeNextSecure, &r);
                        }
                        mi++;
                    }
                }
            }
            Responder(MDNS& mdns, const Header& header) : _mdns(mdns), _header(header),
                  recordsLengthStatic(3), recordsLength(_mdns._services.size() + recordsLengthStatic),
                  recordsMatcherTop(recordsLength), recordsMatcherEach(recordsLength){};
        } _responder(*this, header);

        /////////////////////////////////////////////
        /////////////////////////////////////////////

        if (UDP_READ_PACKET_CLASS processor(_responder, header); !processor.process())
            return Status::PacketBad;

    } else {
#ifdef DEBUG_MDNS

        log_debug(F("MDNS: packet: debugging, %s / %s:%u"), parseHeader(header).c_str(), UDP_READ_PEER_ADDR().toString().c_str(), UDP_READ_PEER_PORT());

        NameCollector collector(*this, header);    // will do nothing, already did debugging
        UDP_READ_PACKET_CLASS<NameCollector> processor(collector, header);
        if (!processor.process())
            return Status::PacketBad;    // should throw

#endif
    }
    // udp flush already done
    return Status::Success;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

static constexpr uint16_t DNS_COUNT_SINGLE = 1;         // Used for single record responses
static constexpr uint16_t DNS_COUNT_SERVICE = 4;        // Used for service announcements (SRV+TXT+2×PTR)
static constexpr uint16_t DNS_COUNT_A_RECORD = 1;       // A record
static constexpr uint16_t DNS_COUNT_PER_SERVICE = 3;    // SRV + TXT + PTR per service
static constexpr uint16_t DNS_COUNT_DNS_SD_PTR = 1;     // DNS-SD PTR record
static constexpr uint16_t DNS_COUNT_NSEC_RECORD = 1;    // NSEC record with bitmap

MDNS::Status MDNS::_messageSend(const uint16_t xid, const int type, const Service* service) {

    UDP_WRITE_BEGIN();

    Header header{};
    header.xid = htons(xid);
    header.opCode = DNS_OPCODE_QUERY;
    switch (type) {
        case PacketTypeAddressRecord:
        case PacketTypeAddressRelease:
        case PacketTypeReverseRecord:
            header.queryResponse = DNS_QR_RESPONSE;
            header.authoritativeAnswer = DNS_AA_AUTHORITATIVE;
            header.answerCount = htons(DNS_COUNT_A_RECORD);
            header.additionalCount = htons(type != PacketTypeReverseRecord ? 0 : DNS_COUNT_A_RECORD);    // A record as additional
            break;
        case PacketTypeServiceRecord:
        case PacketTypeServiceRelease:
            header.queryResponse = DNS_QR_RESPONSE;
            header.authoritativeAnswer = DNS_AA_AUTHORITATIVE;
            header.answerCount = htons(DNS_COUNT_PER_SERVICE);
            header.additionalCount = htons(DNS_COUNT_DNS_SD_PTR + DNS_COUNT_A_RECORD);    // DNS-SD + A record as additional
            break;
        case PacketTypeCompleteRecord:
        case PacketTypeCompleteRelease:
            header.queryResponse = DNS_QR_RESPONSE;
            header.authoritativeAnswer = DNS_AA_AUTHORITATIVE;
            header.answerCount = htons(DNS_COUNT_A_RECORD + (_services.empty() ? 0 : (_services.size() * DNS_COUNT_PER_SERVICE + _serviceTypes.size() * DNS_COUNT_DNS_SD_PTR)));
            break;
        case PacketTypeProbe:
            header.queryResponse = DNS_QR_QUERY;
            header.authoritativeAnswer = DNS_AA_NON_AUTHORITATIVE;
            header.queryCount = htons(DNS_COUNT_SINGLE);
            header.authorityCount = htons(DNS_COUNT_A_RECORD + (_services.empty() ? 0 : (_services.size() * DNS_COUNT_PER_SERVICE + _serviceTypes.size() * DNS_COUNT_DNS_SD_PTR)));
            break;
        case PacketTypeNextSecure:
            header.queryResponse = DNS_QR_RESPONSE;
            header.authoritativeAnswer = DNS_AA_AUTHORITATIVE;
            header.answerCount = htons(DNS_COUNT_NSEC_RECORD);
            header.additionalCount = htons(service ? 0 : DNS_COUNT_A_RECORD);    // A record as additional
            break;
        default: break;
    }

    UDP_WRITE_DATA(reinterpret_cast<uint8_t*>(&header), sizeof(Header));

    switch (type) {

        case PacketTypeAddressRecord:
            log_debug(F("MDNS: packet: sending Address record, ip=%s, name=%s"), IPAddress(_addr).toString().c_str(), _fqhn.c_str());
            _writeAddressRecord(_ttls.announce);
            break;
        case PacketTypeAddressRelease:
            log_debug(F("MDNS: packet: sending Address release, ip=%s, name=%s"), IPAddress(_addr).toString().c_str(), _fqhn.c_str());
            _writeAddressRecord(_ttls.goodbye);
            break;
        case PacketTypeReverseRecord:
            log_debug(F("MDNS: packet: sending Reverse record, ip=%s, name=%s"), IPAddress(_addr).toString().c_str(), _fqhn.c_str());
            _writeReverseRecord(_ttls.announce);
            break;

        case PacketTypeServiceRecord:
            assert(service != nullptr);
            log_debug(F("MDNS: packet: sending Service record %s/%u/%s/%s/[%d]"), Service::toString(service->proto).c_str(), service->port, service->name.c_str(), service->_serv.c_str(), service->text.size());
            _writeServiceRecord(*service, _ttls.announce);
            break;
        case PacketTypeServiceRelease:
            assert(service != nullptr);
            log_debug(F("MDNS: packet: sending Service release %s/%u/%s/%s/[%d]"), Service::toString(service->proto).c_str(), service->port, service->name.c_str(), service->_serv.c_str(), service->text.size());
            _writeServiceRecord(*service, _ttls.goodbye);
            break;

        case PacketTypeCompleteRecord:
            log_debug(F("MDNS: packet: sending Complete record, ip=%s, name=%s, arpa=%s, services=%d, serviceTypes=%d"), IPAddress(_addr).toString().c_str(), _fqhn.c_str(), _arpa.c_str(), _services.size(), _serviceTypes.size());
            _writeCompleteRecord(_ttls.announce);
            break;
        case PacketTypeCompleteRelease:
            log_debug(F("MDNS: packet: sending Complete release, ip=%s, name=%s, arpa=%s, services=%d, serviceTypes=%d"), IPAddress(_addr).toString().c_str(), _fqhn.c_str(), _arpa.c_str(), _services.size(), _serviceTypes.size());
            _writeCompleteRecord(_ttls.goodbye);
            break;

        case PacketTypeProbe:
            log_debug(F("MDNS: packet: sending Probe query, name=%s"), _fqhn.c_str());
            _writeProbeRecord(_ttls.probe);
            break;

        case PacketTypeNextSecure:
            log_debug(F("MDNS: packet: sending NextSecure for supported types"));
            _writeNextSecureRecord(service ? service->_fqsn : _fqhn, { DNS_RECORD_PTR, DNS_RECORD_SRV, service ? DNS_RECORD_TXT : DNS_RECORD_A }, _ttls.announce, service ? true : false);
            break;
        default: break;
    }

    UDP_WRITE_END();

    return Status::Success;
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

static void encodeUint16(uint8_t* ptr, const uint16_t val) {
    *(reinterpret_cast<uint16_t*>(ptr)) = htons(val);
}

static void encodeUint32(uint8_t* ptr, const uint32_t val) {
    *(reinterpret_cast<uint32_t*>(ptr)) = htonl(val);
}

//

static void writeControlBytes(UDP* _udp, const uint8_t byte1, const uint8_t byte2, const uint8_t byte3, const uint8_t byte4, const uint32_t ttl) {
    uint8_t buffer[8];
    buffer[0] = byte1;
    buffer[1] = byte2;
    buffer[2] = byte3;
    buffer[3] = byte4;
    encodeUint32(&buffer[4], ttl);
    UDP_WRITE_DATA(buffer, 8);
}

static void writeServiceBytes(UDP* _udp, const uint16_t priority, const uint16_t weight, const uint16_t port) {
    uint8_t buffer[6];
    encodeUint16(&buffer[0], priority);
    encodeUint16(&buffer[2], weight);
    encodeUint16(&buffer[4], port);
    UDP_WRITE_DATA(buffer, 2 + 2 + 2);
}

static void writeLength(UDP* _udp, const uint16_t length) {
    uint8_t buffer[2];
    encodeUint16(buffer, length);
    UDP_WRITE_DATA(buffer, 2);
}

static void writeAddressLengthAndContent(UDP* _udp, const IPAddress& address) {
    const uint8_t buffer[4] = { address[0], address[1], address[2], address[3] };
    writeLength(_udp, 4);
    UDP_WRITE_DATA(buffer, 4);
}

static void writeStringLengthAndContent(UDP* _udp, const String& str, const size_t max) {
    const auto size = static_cast<uint8_t>(std::min(str.length(), max));
    UDP_WRITE_BYTE(size);
    UDP_WRITE_DATA(reinterpret_cast<const uint8_t*>(str.c_str()), size);
}

// TODO compression
static void writeDNSName(UDP* _udp, const String& name) {
    const size_t len = std::min(name.length(), DNS_LABEL_LENGTH_MAX);
    if (!len)
        UDP_WRITE_BYTE(0);
    else {
        uint8_t buffer[len + 2];    // stack usage up to ~64 bytes
        uint16_t write_pos = 1, length_pos = 0;
        for (size_t i = 0; i < len; i++) {
            if (name[i] == '.') {
                buffer[length_pos] = static_cast<uint8_t>(write_pos - (length_pos + 1));
                length_pos = write_pos;
            } else
                buffer[write_pos] = name[i];
            write_pos++;
        }
        if ((write_pos - (length_pos + 1)) > 0) {
            buffer[length_pos] = static_cast<uint8_t>(write_pos - (length_pos + 1));
            // length_pos = write_pos;
            write_pos++;
        }
        buffer[--write_pos] = 0;    // null terminator
        UDP_WRITE_DATA(buffer, write_pos + 1);
    }
}

static size_t sizeofDNSName(const String& name) {
    return name.length() + 2;    // string length + length byte + null terminator ('.'s just turn into byte lengths)
}

//

static void writeNameLengthAndContent(UDP* _udp, const String& name) {
    writeLength(_udp, name.length() + 2);
    writeDNSName(_udp, name);
}

static void write(UDP* _udp, const DNSBitmap& bitmap) {
    UDP_WRITE_DATA(bitmap.data(), bitmap.size());
}

static void write(UDP* _udp, const MDNS::Service::TXT& record) {
    if (record.entries().empty()) {
        writeLength(_udp, DNS_TXT_EMPTY_LENGTH);
        UDP_WRITE_BYTE(DNS_TXT_EMPTY_CONTENT);
    } else {
        const uint16_t length = record.length();
        writeLength(_udp, length);
        for (const auto&[key, value, binary] : record.entries()) {
            String encoded;
            encoded.reserve(MDNS::Service::TXT::TOTAL_LENGTH_MAX + 1);
            encoded += key;
            if (!value.empty()) {
                encoded += '=';
                if (binary) {
                    std::vector<char> buffer(Base64::length(value.size()) + 1);
                    if (Base64::encode(value.data(), value.size(), buffer.data(), buffer.size()))
                        encoded += String(buffer.data());
                } else
                    encoded += String(reinterpret_cast<const char*>(value.data()), value.size());
            }
            writeStringLengthAndContent(_udp, encoded, MDNS::Service::TXT::TOTAL_LENGTH_MAX);
        }
    }
}

// -----------------------------------------------------------------------------------------------

static void writePTRRecord(UDP* _udp, const String& name, const String& target, const uint8_t cacheFlush, const uint32_t ttl) {
    writeDNSName(_udp, name);
    writeControlBytes(_udp, DNS_RECORD_HI, DNS_RECORD_PTR, cacheFlush, DNS_CLASS_IN, ttl);
    writeNameLengthAndContent(_udp, target);
}
static void writeARecord(UDP* _udp, const String& name, const IPAddress& addr, const uint8_t cacheFlush, const uint32_t ttl) {
    writeDNSName(_udp, name);
    writeControlBytes(_udp, DNS_RECORD_HI, DNS_RECORD_A, cacheFlush, DNS_CLASS_IN, ttl);
    writeAddressLengthAndContent(_udp, addr);
}
static void writeANYRecord(UDP* _udp, const String& name, const IPAddress& addr) {
    writeDNSName(_udp, name);
    writeControlBytes(_udp, DNS_RECORD_HI, DNS_RECORD_ANY, DNS_CACHE_NO_FLUSH, DNS_CLASS_IN, 0);    // Always CACHE_NO_FLUSH and TTL=0 for probe queries
    writeAddressLengthAndContent(_udp, addr);
}
static void writeNSECRecord(UDP* _udp, const String& name, const DNSBitmap& bitmap, const uint8_t cacheFlush, const uint32_t ttl) {
    writeDNSName(_udp, name);
    writeControlBytes(_udp, DNS_RECORD_HI, DNS_RECORD_NSEC, cacheFlush, DNS_CLASS_IN, ttl);
    writeLength(_udp, sizeofDNSName(name) + bitmap.size());
    writeDNSName(_udp, name);
    write(_udp, bitmap);
}
static void writeSRVRecord(UDP* _udp, const String& name, const String& fqhn, const uint16_t port, const MDNS::Service::Config& config, const uint8_t cacheFlush, const uint32_t ttl) {
    writeDNSName(_udp, name);
    writeControlBytes(_udp, DNS_RECORD_HI, DNS_RECORD_SRV, cacheFlush, DNS_CLASS_IN, ttl);
    writeLength(_udp, 2 + 2 + 2 + sizeofDNSName(fqhn));
    writeServiceBytes(_udp, config.priority, config.weight, port);
    writeDNSName(_udp, fqhn);
}
static void writeTXTRecord(UDP* _udp, const String& name, const MDNS::Service::TXT& text, const uint8_t cacheFlush, const uint32_t ttl) {
    writeDNSName(_udp, name);
    writeControlBytes(_udp, DNS_RECORD_HI, DNS_RECORD_TXT, cacheFlush, DNS_CLASS_IN, ttl);
    write(_udp, text);
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

void MDNS::_writeAddressRecord(const uint32_t ttl) const {

    // 1. A record for Hostname -> IP Address
    writeARecord(_udp, _fqhn, _addr, configureCacheFlush(DNSRecordUniqueness::Unique), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));
}

// -----------------------------------------------------------------------------------------------

void MDNS::_writeReverseRecord(const uint32_t ttl) const {

    // 1. PTR record for Reverse IP Address -> Hostname
    writePTRRecord(_udp, _arpa, _fqhn, configureCacheFlush(DNSRecordUniqueness::Shared), configureTTL(DNSRecordUniqueness::Shared, _ttls, ttl));
    // 2. A record for Hostname -> IP Address
    writeARecord(_udp, _fqhn, _addr, configureCacheFlush(DNSRecordUniqueness::Unique), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));
}

// -----------------------------------------------------------------------------------------------

void MDNS::_writeServiceRecord(const Service& service, const uint32_t ttl) const {

    // 1. SRV record for Service -> Hostname
    writeSRVRecord(_udp, service._fqsn, _fqhn, service.port, service.config, configureCacheFlush(DNSRecordUniqueness::Unique), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));
    // 2. TXT record for Service (no target)
    writeTXTRecord(_udp, service._fqsn, service.text, configureCacheFlush(DNSRecordUniqueness::Contextual), configureTTL(DNSRecordUniqueness::Contextual, _ttls, ttl));
    // 3. PTR record for Service Type -> Service
    writePTRRecord(_udp, service._serv, service._fqsn, configureCacheFlush(DNSRecordUniqueness::Shared), configureTTL(DNSRecordUniqueness::Shared, _ttls, ttl));
    // // x. PTR records for Sub Service Types
    // for (const auto& subtype : service.config.subtypes)
    //     _writePTRRecord(_udp, subtype + "._sub." + service._serv, service._fqsn, _configureCacheFlush(DNSRecordUniqueness::Shared, isProbing), _configureTTL(DNSRecordUniqueness::Shared, _ttls, ttl));

    // 4. PTR record for DNS-SD => Service Type
    writePTRRecord(_udp, SERVICE_SD_fqsn, service._serv, configureCacheFlush(DNSRecordUniqueness::Shared), configureTTL(DNSRecordUniqueness::Shared, _ttls, ttl));
    // 5. A record for Hostname -> IP Address
    writeARecord(_udp, _fqhn, _addr, configureCacheFlush(DNSRecordUniqueness::Unique), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));
}

static size_t sizeofServiceRecord(const MDNS::Service& service, const String& fqhn) {
    size_t size = 0;
    size += sizeofDNSName(service._fqsn) + DNS_RECORD_HEADER_SIZE + DNS_SRV_DETAILS_SIZE + sizeofDNSName(fqhn);    // SRV
    size += sizeofDNSName(service._fqsn) + DNS_RECORD_HEADER_SIZE + service.text.length();                          // TXT
    size += sizeofDNSName(service._serv) + DNS_RECORD_HEADER_SIZE + sizeofDNSName(service._fqsn);                  // PTR SRV
    // size += service.config.subtypes.empty() ? 0 : std::accumulate(service.config.subtypes.begin(), service.config.subtypes.end(), static_cast<size_t>(0), [&](size_t size, const auto& subtype) {
    //     return size + _sizeofDNSName(subtype + "._sub." + service._serv) + DNS_RECORD_HEADER_SIZE + _sizeofDNSName(service._fqsn);
    // });
    return size;
}

// -----------------------------------------------------------------------------------------------

void MDNS::_writeCompleteRecord(const uint32_t ttl) const {

    // 1. A record for Hostname -> IP Address
    writeARecord(_udp, _fqhn, _addr, configureCacheFlush(DNSRecordUniqueness::Unique), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));

    if (!_services.empty()) {
        // 3-N service records
        for (const auto& service : _services) {
            // 1. SRV record for Service -> Hostname
            writeSRVRecord(_udp, service._fqsn, _fqhn, service.port, service.config, configureCacheFlush(DNSRecordUniqueness::Unique), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));
            // 2. TXT record for Service (no target)
            writeTXTRecord(_udp, service._fqsn, service.text, configureCacheFlush(DNSRecordUniqueness::Contextual), configureTTL(DNSRecordUniqueness::Contextual, _ttls, ttl));
            // 3. PTR record for Service Type -> Service
            writePTRRecord(_udp, service._serv, service._fqsn, configureCacheFlush(DNSRecordUniqueness::Shared), configureTTL(DNSRecordUniqueness::Shared, _ttls, ttl));
        }

        // N-O PTR records for DNS-SD => Service Type
        for (const auto& serviceType : _serviceTypes)
            writePTRRecord(_udp, SERVICE_SD_fqsn, serviceType, configureCacheFlush(DNSRecordUniqueness::Shared), configureTTL(DNSRecordUniqueness::Shared, _ttls, ttl));
    }
}

static size_t sizeofCompleteRecord(const MDNS::Services& services, const MDNS::ServiceTypes& serviceTypes, const String& fqhn) {
    size_t size = 0;
    size += sizeofDNSName(fqhn) + DNS_RECORD_HEADER_SIZE + 4;    // PTR IP
    size += services.empty() ? 0 : std::accumulate(services.begin(), services.end(), static_cast<size_t>(0), [&](size_t size, const MDNS::Service& service) {
        size += sizeofDNSName(service._fqsn) + DNS_RECORD_HEADER_SIZE + DNS_SRV_DETAILS_SIZE + sizeofDNSName(fqhn);    // SRV
        size += sizeofDNSName(service._fqsn) + DNS_RECORD_HEADER_SIZE + service.text.length();                          // TXT
        size += sizeofDNSName(service._serv) + DNS_RECORD_HEADER_SIZE + sizeofDNSName(service._fqsn);                  // PTR SRV
        return size;
    });
    size += serviceTypes.empty() ? 0 : std::accumulate(serviceTypes.begin(), serviceTypes.end(), static_cast<size_t>(0), [&](size_t size, const String& serviceType) {
        return size + sizeofDNSName(SERVICE_SD_fqsn) + DNS_RECORD_HEADER_SIZE + sizeofDNSName(serviceType);    // PTR SD
    });
    return size;
}

// -----------------------------------------------------------------------------------------------

void MDNS::_writeProbeRecord(const uint32_t ttl) const {
    static constexpr bool isProbing = true;

    // 1. ANY record for Hostname -> IP Address
    writeANYRecord(_udp, _fqhn, _addr);
    // 2. A record for Hostname -> IP Address
    writeARecord(_udp, _fqhn, _addr, configureCacheFlush(DNSRecordUniqueness::Unique, isProbing), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));

    if (!_services.empty()) {
        // 3-N service records
        for (const auto& service : _services) {
            // 1. SRV record for Service -> Hostname
            writeSRVRecord(_udp, service._fqsn, _fqhn, service.port, service.config, configureCacheFlush(DNSRecordUniqueness::Unique, isProbing), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));
            // 2. TXT record for Service (no target)
            writeTXTRecord(_udp, service._fqsn, service.text, configureCacheFlush(DNSRecordUniqueness::Contextual, isProbing), configureTTL(DNSRecordUniqueness::Contextual, _ttls, ttl));
            // 3. PTR record for Service Type -> Service
            writePTRRecord(_udp, service._serv, service._fqsn, configureCacheFlush(DNSRecordUniqueness::Shared, isProbing), configureTTL(DNSRecordUniqueness::Shared, _ttls, ttl));
        }

        // N-O PTR records for DNS-SD => Service Type
        for (const auto& serviceType : _serviceTypes)
            writePTRRecord(_udp, SERVICE_SD_fqsn, serviceType, configureCacheFlush(DNSRecordUniqueness::Shared, isProbing), configureTTL(DNSRecordUniqueness::Shared, _ttls, ttl));
    }
}

// -----------------------------------------------------------------------------------------------

void MDNS::_writeNextSecureRecord(const String& name, const std::initializer_list<uint8_t>& types, const uint32_t ttl, const bool includeAdditional) const {
    DNSBitmap bitmap(types);
    // 1. NSEC record with Service bitmap
    writeNSECRecord(_udp, name, bitmap, configureCacheFlush(DNSRecordUniqueness::Unique), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));

    if (includeAdditional) {
        // 2. A record for Hostname -> IP Address
        writeARecord(_udp, _fqhn, _addr, configureCacheFlush(DNSRecordUniqueness::Unique), configureTTL(DNSRecordUniqueness::Unique, _ttls, ttl));
    }
}

