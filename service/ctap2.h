#pragma once
#include <cstdint>
#include <vector>

std::vector<uint8_t> ctap2_handle(uint8_t cmd, const uint8_t* data, size_t len);
