// Copyright (c) 2025 by Dan Luca. All rights reserved.
//

#include "WebRequest.h"
#include <LogProxy.h>
#include <MD5Builder.h>
#include <libb64/cencode.h>

static constexpr char qop_auth[] PROGMEM = "qop=auth";
static constexpr char qop_auth_quoted[] PROGMEM = "qop=\"auth\"";

WebRequest::~WebRequest() {
    for (const auto &a: _requestArgs)
        delete a;
    for (const auto &h: _headers)
        delete h;
    _requestArgs.clear();
    _headers.clear();
}

String WebRequest::extractParam(const String &authReq, const String &param, const char delimit) {
    const int _begin = authReq.indexOf(param);
    if (_begin == -1) {
        return "";
    }
    return authReq.substring(_begin + param.length(), authReq.indexOf(delimit, _begin + param.length()));
}

static String md5str(const String &in) {
    char out[33] = {0};
    MD5Builder _ctx{};
    uint8_t _buf[16] = {};
    _ctx.begin();
    _ctx.add(reinterpret_cast<const uint8_t *>(in.c_str()), in.length());
    _ctx.calculate();
    _ctx.getBytes(_buf);
    for (uint8_t i = 0; i < 16; i++) {
        sprintf(out + (i * 2), "%02x", _buf[i]);
    }
    out[32] = 0;
    return {out};
}

/**
 * Revisit this code for security, robustness. A web server running on embedded platform is not expected to
 * manage user credentials dynamically, perhaps the authentication needs can be met in a more robust manner.
 * @param username username
 * @param password password
 * @return true if username/password match
 */
bool WebRequest::authenticate(const char *username, const char *password) const {
    if (hasHeader(FPSTR (AUTHORIZATION_HEADER))) {
        if (String authReq = header(FPSTR (AUTHORIZATION_HEADER)); authReq.startsWith(F("Basic"))) {
            authReq = authReq.substring(6);
            authReq.trim();
            size_t toencodeLen = strlen(username) + strlen(password) + 1;
            auto toencode = new char[toencodeLen + 1];
            auto encoded = new char[base64_encode_expected_len(toencodeLen) + 1];
            sprintf(toencode, "%s:%s", username, password);
            if (base64_encode_chars(toencode, static_cast<int>(toencodeLen), encoded) > 0 && authReq == encoded) {
                delete[] toencode;
                delete[] encoded;
                return true;
            }
            delete[] toencode;
            delete[] encoded;
        } else if (authReq.startsWith(F("Digest"))) {
            authReq = authReq.substring(7);
            log_debug("%s", authReq.c_str());
            if (String _username = extractParam(authReq, F("username=\""), '\"');
                !_username.length() || _username != String(username)) {
                return false;
            }
            // extracting required parameters for RFC 2069 simpler Digest
            const String _realm = extractParam(authReq, F("realm=\""), '\"');
            const String _nonce = extractParam(authReq, F("nonce=\""), '\"');
            const String _uri = extractParam(authReq, F("uri=\""), '\"');
            const String _response = extractParam(authReq, F("response=\""), '\"');
            const String _opaque = extractParam(authReq, F("opaque=\""), '\"');

            if ((!_realm.length()) || (!_nonce.length()) || (!_uri.length()) || (!_response.length()) || (!_opaque.
                    length())) {
                return false;
            }
            if ((_opaque != _sOpaque) || (_nonce != _sNonce) || (_realm != _sRealm)) {
                return false;
            }
            // parameters for the RFC 2617 newer Digest
            String _nc, _cnonce;
            if (authReq.indexOf(FPSTR (qop_auth)) != -1 || authReq.indexOf(FPSTR (qop_auth_quoted)) != -1) {
                _nc = extractParam(authReq, F("nc="), ',');
                _cnonce = extractParam(authReq, F("cnonce=\""), '\"');
            }
            const String H1 = md5str(String(username) + ':' + _realm + ':' + String(password));
            log_debug("Hash of user:realm:pass=%s", H1.c_str());
            String H2 = "";
            if (_method == HTTP_GET) {
                H2 = md5str(String(F("GET:")) + _uri);
            } else if (_method == HTTP_POST) {
                H2 = md5str(String(F("POST:")) + _uri);
            } else if (_method == HTTP_PUT) {
                H2 = md5str(String(F("PUT:")) + _uri);
            } else if (_method == HTTP_DELETE) {
                H2 = md5str(String(F("DELETE:")) + _uri);
            } else {
                H2 = md5str(String(F("GET:")) + _uri);
            }
            log_debug("Hash of GET:uri=%s", H2.c_str());
            String _responsecheck = "";
            if (authReq.indexOf(FPSTR (qop_auth)) != -1 || authReq.indexOf(FPSTR (qop_auth_quoted)) != -1) {
                _responsecheck = md5str(H1 + ':' + _nonce + ':' + _nc + ':' + _cnonce + F(":auth:") + H2);
            } else {
                _responsecheck = md5str(H1 + ':' + _nonce + ':' + H2);
            }
            log_debug("The Proper response=%s", _responsecheck.c_str());
            if (_response == _responsecheck) {
                return true;
            }
        }
    }
    return false;
}

String WebRequest::pathArg(const unsigned int i) const {
    if (i < _pathArgs.size())
        return _pathArgs[i];
    return "";
}

String WebRequest::arg(const String &name) const {
    for (const auto &a: _requestArgs) {
        if (a->key == name) {
            return a->value;
        }
    }
    return "";
}

String WebRequest::arg(const int i) const {
    return i < _requestArgs.size() ? _requestArgs[i]->value : "";
}

String WebRequest::argName(const int i) const {
    return i < _requestArgs.size() ? _requestArgs[i]->key : "";
}

size_t WebRequest::argsCount() const {
    return _requestArgs.size();
}

bool WebRequest::hasArg(const String &name) const {
    for (const auto &a: _requestArgs) {
        if (a->key == name)
            return true;
    }
    return false;
}


String WebRequest::header(const String &name) const {
    for (const auto &h: _headers)
        if (h->key.equalsIgnoreCase(name))
            return h->value;
    return "";
}

String WebRequest::header(const int i) const {
    return i < _headers.size() ? _headers[i]->value : "";
}

String WebRequest::headerName(const int i) const {
    return i < _headers.size() ? _headers[i]->key : "";
}

size_t WebRequest::headersCount() const {
    return _headers.size();
}

bool WebRequest::hasHeader(const String &name) const {
    for (const auto &h: _headers)
        if (h->key.equalsIgnoreCase(name))
            return true;
    return false;
}
