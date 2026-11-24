// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

#include <HTTPClient.h>
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
    HTTPClient client;
    client.setTimeout(750);
    client.setReuse(false);
    if (!client.begin(socket, ip.toString(), 80, "/mesh")) return -1;
    client.addHeader("Content-Type", "application/json");
    const int status = client.PUT(body);
    const int size = client.getSize();
    if (status > 0 && size >= 0 && size <= static_cast<int>(mesh::maxBodyBytes))
        response = client.getString();
    client.end();
    return status;
}
