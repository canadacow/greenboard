#include "host_platform/thread_util.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace bench {

void thread_set_time_critical() {
    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL))
        spdlog::debug("[thread_util] set THREAD_PRIORITY_TIME_CRITICAL");
    else
        spdlog::warn("[thread_util] SetThreadPriority failed: {}", GetLastError());
}

void thread_pin_to_pcores() {
    // Query CPU set information to find P-cores (EfficiencyClass == 0).
    DWORD len = 0;
    GetSystemCpuSetInformation(nullptr, 0, &len, GetCurrentProcess(), 0);
    if (len == 0) return;

    auto buf = std::make_unique<uint8_t[]>(len);
    if (!GetSystemCpuSetInformation(
            reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buf.get()),
            len, &len, GetCurrentProcess(), 0))
        return;

    // Find the max EfficiencyClass -- P-cores have the HIGHEST value on
    // Intel hybrid (Arrow Lake, etc.).  Microsoft docs say "higher class =
    // more efficient" but in practice the performance cores report the
    // highest EfficiencyClass number.
    BYTE max_class = 0;
    auto* ptr = buf.get();
    auto* end_ptr = ptr + len;
    while (ptr < end_ptr) {
        auto* info = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(ptr);
        if (info->Type == CpuSetInformation && info->CpuSet.EfficiencyClass > max_class)
            max_class = info->CpuSet.EfficiencyClass;
        ptr += info->Size;
    }

    if (max_class == 0) {
        spdlog::debug("[thread_util] no hybrid topology detected, skipping affinity");
        return;
    }

    // Select cores with the highest efficiency class (P-cores).
    DWORD_PTR mask = 0;
    int pcore_count = 0, ecore_count = 0;
    ptr = buf.get();
    while (ptr < end_ptr) {
        auto* info = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(ptr);
        if (info->Type == CpuSetInformation) {
            if (info->CpuSet.EfficiencyClass == max_class) {
                mask |= (1ULL << info->CpuSet.LogicalProcessorIndex);
                ++pcore_count;
            } else {
                ++ecore_count;
            }
        }
        ptr += info->Size;
    }

    spdlog::debug("[thread_util] detected {} P-core and {} E-core logical processors",
                 pcore_count, ecore_count);

    if (SetThreadAffinityMask(GetCurrentThread(), mask))
        spdlog::debug("[thread_util] pinned to P-cores, mask=0x{:X}", mask);
    else
        spdlog::debug("[thread_util] SetThreadAffinityMask failed: {}", GetLastError());
}

} // namespace bench

#else
// Stubs for non-Windows platforms.
namespace bench {
void thread_set_time_critical() {}
void thread_pin_to_pcores() {}
} // namespace bench
#endif
