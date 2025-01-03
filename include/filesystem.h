// Copyright (c) 2024 by Dan Luca. All rights reserved.
//
#pragma once
#ifndef RP2040_LIGHTFX_EARLE_FILESYSTEM_H
#define RP2040_LIGHTFX_EARLE_FILESYSTEM_H

void fsSetup();

size_t readTextFile(const char *fname, String *s);
size_t writeTextFile(const char *fname, String *s);
bool removeFile(const char *fname);



#endif //RP2040_LIGHTFX_EARLE_FILESYSTEM_H
