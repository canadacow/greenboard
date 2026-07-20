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

// Pin the calling thread to cores of one efficiency class.
// P-cores report the HIGHEST EfficiencyClass value on Intel hybrid
// (Arrow Lake, etc.) -- Microsoft docs say "higher class = more
// efficient" but in practice the performance cores report the highest
// EfficiencyClass number. want_pcores selects that class; otherwise
// everything below it (E-cores).
static void pin_to_core_class(bool want_pcores) {
    DWORD len = 0;
    GetSystemCpuSetInformation(nullptr, 0, &len, GetCurrentProcess(), 0);
    if (len == 0) return;

    auto buf = std::make_unique<uint8_t[]>(len);
    if (!GetSystemCpuSetInformation(
            reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buf.get()),
            len, &len, GetCurrentProcess(), 0))
        return;

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

    DWORD_PTR mask = 0;
    int pcore_count = 0, ecore_count = 0;
    ptr = buf.get();
    while (ptr < end_ptr) {
        auto* info = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(ptr);
        if (info->Type == CpuSetInformation) {
            bool is_pcore = (info->CpuSet.EfficiencyClass == max_class);
            if (is_pcore) ++pcore_count; else ++ecore_count;
            if (is_pcore == want_pcores)
                mask |= (1ULL << info->CpuSet.LogicalProcessorIndex);
        }
        ptr += info->Size;
    }

    spdlog::debug("[thread_util] detected {} P-core and {} E-core logical processors",
                 pcore_count, ecore_count);

    if (mask == 0) {
        spdlog::warn("[thread_util] no {} cores found, skipping affinity",
                     want_pcores ? "performance" : "efficiency");
        return;
    }

    if (SetThreadAffinityMask(GetCurrentThread(), mask))
        spdlog::info("[thread_util] pinned to {}-cores, mask=0x{:X}",
                     want_pcores ? "P" : "E", mask);
    else
        spdlog::debug("[thread_util] SetThreadAffinityMask failed: {}", GetLastError());
}

void thread_pin_to_pcores() { pin_to_core_class(true); }
void thread_pin_to_ecores() { pin_to_core_class(false); }

} // namespace bench

#else
// Stubs for non-Windows platforms.
namespace bench {
void thread_set_time_critical() {}
void thread_pin_to_pcores() {}
void thread_pin_to_ecores() {}
} // namespace bench
#endif
