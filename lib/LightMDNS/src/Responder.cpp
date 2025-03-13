// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "Responder.h"
#include "LightMDNS.hpp"

int Responder::matchStringPart(const char **pCmpStr, int *pCmpLen, const uint8_t *data, const int dataLen) {
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
}

void Responder::process_iscompressed(const uint16_t offs, const DNSSection section, const uint16_t) {
    if (section != DNSSection::Query) return;
    log_debug(F("(%04X)"), offs);
    for (auto& m : recordsMatcherEach)
        if (m.position && m.position != offs)
            m.match = 0;
}

void Responder::process_nocompressed(const String &name, const DNSSection section, const uint16_t) {
    if (section != DNSSection::Query) return;
    log_debug(F("[%s]"), name.c_str());
    for (auto& m : recordsMatcherEach)
        if (!m.requested && m.match)
            m.match &= matchStringPart(&m.name, &m.length, reinterpret_cast<const uint8_t*>(name.c_str()), static_cast<int>(name.length()));
}

void Responder::process_end(const DNSSection section, const uint16_t) {
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
}

void Responder::begin() {
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
}

void Responder::end() const {
    // XXX should coaescle into single response(s)
    // XXX should only have unique service names and match from that
    if (recordsMatcherTop[0].unsupported || recordsMatcherTop[1].unsupported || recordsMatcherTop[2].unsupported) {
        log_debug(F("MDNS: packet: processing, negated[%d/%d/%d]"), recordsMatcherTop[0].unsupported, recordsMatcherTop[1].unsupported, recordsMatcherTop[2].unsupported);
        (void)_mdns._messageSend(_header.xid, PacketTypeNextSecure);
    }
    if (recordsMatcherTop[0].requested) {
        log_debug(F("MDNS: packet: processing, matched[NAME]: %s"), recordsMatcherTop[0].name);
        (void)_mdns._messageSend(_header.xid, PacketTypeAddressRecord);
    }
    if (recordsMatcherTop[1].requested) {
        log_debug(F("MDNS: packet: processing, matched[ADDR]: %s"), recordsMatcherTop[1].name);
        (void)_mdns._messageSend(_header.xid, PacketTypeAddressRecord);
    }
    if (recordsMatcherTop[2].requested) {
        log_debug(F("MDNS: packet: processing, matched[DISC]: %s"), recordsMatcherTop[2].name);
        (void)_mdns._messageSend(_header.xid, PacketTypeCompleteRecord);
    } else {
        size_t mi = 0;
        for (const auto& r : _mdns._services) {
            const auto& m = recordsMatcherTop[mi + recordsLengthStatic];
            if (m.requested) {
                log_debug(F("MDNS: packet: processing, matched[SERV:%d]: %s"), mi, m.name);
                (void)_mdns._messageSend(_header.xid, PacketTypeServiceRecord, &r);
            }
            if (m.unsupported) {
                log_debug(F("MDNS: packet: processing, negated[SERV:%d]: %s"), mi, m.name);
                (void)_mdns._messageSend(_header.xid, PacketTypeNextSecure, &r);
            }
            mi++;
        }
    }
}

Responder::Responder(MDNS &mdns, const Header &header): _mdns(mdns), _header(header), recordsLengthStatic(3), recordsLength(_mdns._services.size() + recordsLengthStatic),
    recordsMatcherTop(recordsLength), recordsMatcherEach(recordsLength) {}

