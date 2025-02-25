// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#ifndef RESPONDER_H
#define RESPONDER_H

#include "DNSSection.h"
#include <vector>

inline static constexpr auto SERVICE_SD_fqsn = "_services._dns-sd._udp.local";

struct matcher_t {
        const char* name{};
        int length{};
        int match = 1;
        uint16_t position = 0;
        bool requested = false, unsupported = false;
};

class MDNS;
struct Responder {
    MDNS& _mdns;
    const Header& _header;
    //
    // this is all horrible and brittle and needs replacement, but is getting there ...
    const size_t recordsLengthStatic, recordsLength;

    std::vector<matcher_t> recordsMatcherTop{}, recordsMatcherEach{};
    uint16_t _starting{};
    uint8_t _control[4]{};

    int matchStringPart(const char** pCmpStr, int* pCmpLen, const uint8_t* data, int dataLen);;
    [[nodiscard]] String name() const {
        return "UNSUPPORTED";
    }
    void process_iscompressed(uint16_t offs, DNSSection section, uint16_t);
    void process_nocompressed(const String& name, DNSSection section, uint16_t);
    void process_begin(const DNSSection section, const uint16_t starting) {
        if (section != DNSSection::Query) return;
        _starting = starting;
    }
    void process_update(const DNSSection section, const uint8_t control[4]) {
        if (section != DNSSection::Query) return;
        memcpy(_control, control, sizeof(_control));
    }
    void process_end(DNSSection section, uint16_t);
    void begin();
    void end() const;

    Responder(MDNS& mdns, const Header& header);
};

#endif //RESPONDER_H
