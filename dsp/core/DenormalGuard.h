#pragma once

// =============================================================================
//  ScopedDenormalDisable — RAII denormal number suppression guard
//
//  Simply place on the stack at the beginning of processBlock to enable FTZ/DAZ,
//  and automatically restore flags when scope ends.
//  Auto-detects x86 (SSE), ARM (NEON/AArch64). No-op on unsupported environments.
// =============================================================================

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
  #include <xmmintrin.h>
  #define PATINA_DENORMAL_X86 1
#endif

#include <cstdint>

namespace patina {

class ScopedDenormalDisable
{
public:
    ScopedDenormalDisable() noexcept
    {
#if defined(PATINA_DENORMAL_X86)
        oldMxcsr_ = _mm_getcsr();
        _mm_setcsr(oldMxcsr_ | 0x8040u);          // FTZ (bit 15) | DAZ (bit 6)
#elif defined(__aarch64__)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        oldFpcr_ = fpcr;
        asm volatile("msr fpcr, %0" :: "r"(fpcr | (uint64_t(1) << 24)));  // FZ bit
#elif defined(__ARM_NEON) || defined(__arm__)
        uint32_t fpscr;
        asm volatile("vmrs %0, fpscr" : "=r"(fpscr));
        oldFpcr_ = fpscr;
        asm volatile("vmsr fpscr, %0" :: "r"(fpscr | (uint32_t(1) << 24)));
#endif
    }

    ~ScopedDenormalDisable() noexcept
    {
#if defined(PATINA_DENORMAL_X86)
        _mm_setcsr(oldMxcsr_);
#elif defined(__aarch64__)
        asm volatile("msr fpcr, %0" :: "r"(oldFpcr_));
#elif defined(__ARM_NEON) || defined(__arm__)
        asm volatile("vmsr fpscr, %0" :: "r"(static_cast<uint32_t>(oldFpcr_)));
#endif
    }

    ScopedDenormalDisable(const ScopedDenormalDisable&) = delete;
    ScopedDenormalDisable& operator=(const ScopedDenormalDisable&) = delete;

private:
#if defined(PATINA_DENORMAL_X86)
    unsigned int oldMxcsr_ = 0;
#elif defined(__aarch64__) || defined(__ARM_NEON) || defined(__arm__)
    uint64_t oldFpcr_ = 0;
#endif
};

} // namespace patina
