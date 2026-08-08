#pragma once

#include <atomic>
#include <cstddef>

// AllocationCounter — a small, opt-in global heap-allocation counter, NOT
// a general allocator framework: global operator new/delete are overridden
// (see AllocationCounter.cpp) to increment plain atomics and then forward
// straight to malloc/free, with counting gated by g_enabled so there is
// zero behavioral difference and near-zero overhead when disabled (the
// default). Used only by GlRestorationHarness to get MEASURED steady-state
// per-frame allocation counts (Development Stream 1 §14) rather than
// asserted/claimed ones — this is process-wide (every operator new/delete
// call in the binary, OF-internal or not), which is the point: it reports
// real per-frame allocation churn, not just the parts this scaffold's own
// code happens to call.
namespace alloccounter {

inline std::atomic<bool> g_enabled{false};
inline std::atomic<long long> g_newCount{0};
inline std::atomic<long long> g_deleteCount{0};

inline void reset() {
	g_newCount.store(0, std::memory_order_relaxed);
	g_deleteCount.store(0, std::memory_order_relaxed);
}

inline void setEnabled(bool enabled) {
	g_enabled.store(enabled, std::memory_order_relaxed);
}

inline long long newCount() {
	return g_newCount.load(std::memory_order_relaxed);
}

inline long long deleteCount() {
	return g_deleteCount.load(std::memory_order_relaxed);
}

} // namespace alloccounter
