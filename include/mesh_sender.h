// MIT License
//
// Copyright (c) by Dan Luca. All rights reserved.
//

#pragma once

// Call from the web/comms task only. Advances at most one peer attempt per call.
// The HTTP transport is synchronous; this is not a nonblocking I/O API.
void serviceMeshClients();
