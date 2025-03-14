#include <Arduino.h>
#include "mimetable.h"

namespace mime {

// Table of extension->MIME strings stored in PROGMEM, needs to be global due to GCC section typing rules
constexpr Entry mimeTable[maxType] = {
    { ".html", "text/html" },
    { ".htm", "text/html" },
    { ".css", "text/css" },
    { ".txt", "text/plain" },
    { ".js", "application/javascript" },
    { ".json", "application/json" },
    { ".png", "image/png" },
    { ".gif", "image/gif" },
    { ".jpg", "image/jpeg" },
    { ".ico", "image/x-icon" },
    { ".svg", "image/svg+xml" },
    { ".ttf", "application/x-font-ttf" },
    { ".otf", "application/x-font-opentype" },
    { ".woff", "application/font-woff" },
    { ".woff2", "application/font-woff2" },
    { ".eot", "application/vnd.ms-fontobject" },
    { ".sfnt", "application/font-sfnt" },
    { ".xml", "text/xml" },
    { ".pdf", "application/pdf" },
    { ".zip", "application/zip" },
    { ".gz", "application/x-gzip" },
    { ".appcache", "text/cache-manifest" },
    { "", "application/octet-stream" }
};

String getContentType(const String& path) {
    for (auto [endsWith, mimeType] : mimeTable) {
        if (path.endsWith(endsWith)) {
            return {mimeType};
        }
    }
    // Fall-through and just return default type
    return {(mimeTable[none].mimeType)};
}
}
