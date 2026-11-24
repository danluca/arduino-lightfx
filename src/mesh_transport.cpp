// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

#include <ArduinoHttpClient.h>
#include "mesh_http.h"

/**
 * Mesh transport layer. Exchange data with a mesh node over HTTP.
 *
 * @param ip IP address of the mesh node.
 * @param body JSON body of the HTTP request.
 * @param response Response body of the HTTP response.
 * @return HTTP status code of the response.
 */
int meshExchange(const IPAddress &ip, const String &body, String &response) {
    WiFiClient socket;
    socket.setConnectionTimeout(750);
    HttpClient client(socket, ip, 80);
    client.setTimeout(750);
    client.setHttpResponseTimeout(750);
    client.noDefaultRequestHeaders();
    int status = -1;
    client.beginRequest();
    if (client.put("/mesh") == HTTP_SUCCESS) {
        client.sendHeader("Host", ip.toString());
        client.sendHeader("Content-Type", "application/json");
        client.sendHeader("Content-Length", body.length());
        client.sendHeader("Connection", "close");
        client.beginBody();
        client.print(body);
        client.endRequest();
        status = client.responseStatusCode();
        const long size = status > 0 ? client.contentLength() : -1;
        if (size >= 0 && size <= static_cast<long>(mesh::maxBodyBytes))
            response = client.responseBody();
    }
    client.stop();
    return status;
}
