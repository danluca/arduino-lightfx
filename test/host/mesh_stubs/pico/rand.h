#pragma once
#include <cstdint>
inline uint64_t get_rand_64() { static uint64_t value = 100; return ++value; }
