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