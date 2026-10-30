#include <cstdio>
#include <vector>
#include "ctap2.h"

int main() {
    auto out = ctap2_handle(0x04, nullptr, 0);
    std::printf("getInfo len=%zu\n", out.size());
    for (size_t i = 0; i < out.size(); ++i) std::printf("%02X ", out[i]);
    std::printf("\n");
    return 0;
}
