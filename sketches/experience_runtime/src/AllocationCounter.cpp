#include "AllocationCounter.h"

#include <cstdlib>
#include <new>

// Global operator new/delete overrides, scoped to this one small
// standalone application only (not linked into any production scene or
// shared/src) — see AllocationCounter.h's header comment.

void* operator new(std::size_t size) {
	if (alloccounter::g_enabled.load(std::memory_order_relaxed)) {
		alloccounter::g_newCount.fetch_add(1, std::memory_order_relaxed);
	}
	void* p = std::malloc(size == 0 ? 1 : size);
	if (!p) {
		throw std::bad_alloc();
	}
	return p;
}

void* operator new[](std::size_t size) {
	return ::operator new(size);
}

void operator delete(void* p) noexcept {
	if (alloccounter::g_enabled.load(std::memory_order_relaxed)) {
		alloccounter::g_deleteCount.fetch_add(1, std::memory_order_relaxed);
	}
	std::free(p);
}

void operator delete(void* p, std::size_t) noexcept {
	::operator delete(p);
}

void operator delete[](void* p) noexcept {
	::operator delete(p);
}

void operator delete[](void* p, std::size_t) noexcept {
	::operator delete(p);
}
