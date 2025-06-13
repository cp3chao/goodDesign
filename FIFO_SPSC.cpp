#include <atomic>
#include <cassert>
#include <memory>
#include <new>
#include <cstddef>
#include <iostream>
template<typename T, std::size_t mask_, typename Alloc>
class FIFO_Base : private Alloc {
public:
    using value_type = T;
    using allocator_traits = std::allocator_traits<Alloc>;
    using size_type = typename allocator_traits::size_type;
    using CursorType = std::atomic<size_type>;
    static_assert(CursorType::is_always_lock_free, "CursorType::is_always_lock_free");
    explicit FIFO_Base(Alloc const& alloc = Alloc{}) :
        Alloc{ alloc }, ring_{ allocator_traits::allocate(*this, mask_ + 1) } {}
    ~FIFO_Base() {
        while (!empty()) {
            element(popCursor_)->~T();
            ++popCursor_;
        }
        allocator_traits::deallocate(*this, ring_, mask_ + 1);
    }
    [[nodiscard]] auto size() const noexcept {
        auto pushCursor = pushCursor_.load(std::memory_order_relaxed);
        auto popCursor = popCursor_.load(std::memory_order_relaxed);
        assert(popCursor <= pushCursor);
        return pushCursor - popCursor;
    }
    [[nodiscard]] auto empty() const noexcept {
        return size() == 0;
    }
    [[nodiscard]] auto push(T const& value) {
        auto pushCursor = pushCursor_.load(std::memory_order_relaxed);
        if (full(pushCursor, popCursorCached_)) {
            popCursorCached_ = popCursor_.load(std::memory_order_acquire);
            if (full(pushCursor, popCursorCached_)) {
                return false;
            }
        }
        new (element(pushCursor)) T(value);
        pushCursor_.store(pushCursor + 1, std::memory_order_release);
        return true;
    }
    [[nodiscard]] auto pop(T& value) {
        auto popCursor = popCursor_.load(std::memory_order_relaxed);
        if (empty(pushCursorCached_, popCursor)) {
            pushCursorCached_ = pushCursor_.load(std::memory_order_acquire);
            if (empty(pushCursorCached_, popCursor)) {
                return false;
            }
        }
        value = *element(popCursor);
        element(popCursor)->~T();
        popCursor_.store(popCursor + 1, std::memory_order_release);
        return true;
    }
private:
    auto full(size_type pushCursor, size_type popCursor) const noexcept {
        return (pushCursor - popCursor) == mask_ + 1;
    }
    static auto empty(size_type pushCursor, size_type popCursor) noexcept {
        return pushCursor == popCursor;
    }
    auto element(size_type cursor) noexcept {
        return &ring_[cursor & mask_];
    }
private:
    T* ring_;
    alignas(std::hardware_destructive_interference_size) CursorType pushCursor_;
    alignas(std::hardware_destructive_interference_size) size_type popCursorCached_ {};
    alignas(std::hardware_destructive_interference_size) CursorType popCursor_;
    alignas(std::hardware_destructive_interference_size) size_type pushCursorCached_ {};
    char padding_[std::hardware_destructive_interference_size - sizeof(size_type)];
};
consteval auto calc_mask(std::size_t min_capacity) noexcept {
    std::size_t index = 0, mask = 0;
    for (std::size_t i = 0; i < sizeof(std::size_t) * 8; ++i) {
        if ((min_capacity >> i) & 1) {
            index = i;
        }
    }
    for (std::size_t i = 0; i <= index; ++i) {
        mask |= std::size_t{ 1 } << i;
    }
    return mask;
}
template<typename T, std::size_t min_capacity, typename Alloc = std::allocator<T>>
class FIFO : public FIFO_Base<T, calc_mask(min_capacity), Alloc> {
    
};


#include <thread>
#include <cstdint>
#include <iostream>
#include "fifo.h"
constexpr int64_t MAXN = int64_t(1e8);
struct Timer {
	std::chrono::steady_clock::time_point start;
	Timer() : start(std::chrono::steady_clock::now()) {}
	~Timer() {
		auto finish = std::chrono::steady_clock::now();
		auto runtime = std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();
		std::cerr << runtime / 1e6 << "s" << std::endl;
	}
};
FIFO<int64_t, 1023> queue;
int main() {
	Timer timer;
	std::thread pop_thread([]() {
		int64_t cnt = 0, last = -1;
		while (cnt != MAXN) {
			int64_t value;
			if (queue.pop(value)) {
				cnt += 1;
				if (value <= last) {
					abort();
				}
				last = value;
			}
		}
		});
	std::thread push_thread([]() {
		for (int64_t i = 1; i <= MAXN;) {
			if (queue.push(i)) {
				i += 1;
			}
		}
		});
	push_thread.join();
	pop_thread.join();
	return 0;
}