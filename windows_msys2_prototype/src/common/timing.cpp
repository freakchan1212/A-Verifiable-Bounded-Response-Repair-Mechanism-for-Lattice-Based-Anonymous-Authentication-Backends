
#include "timing.h"
#include <cstdio>

namespace common {

	
	
	
	
	
	
	Timer::Timer() {
		reset();
	}
	
	void Timer::reset() {
		
		start_ = std::chrono::high_resolution_clock::now();
	}
	
	int64_t Timer::us() const {
		auto now = std::chrono::high_resolution_clock::now();
		
		auto d = std::chrono::duration_cast<std::chrono::microseconds>(now - start_);
		return (int64_t)d.count();
	}
	
	double Timer::ms() const {
		
		return (double)us() / 1000.0;
	}
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	ScopeTimer::ScopeTimer(const char* name, bool enabled)
		: name_(name), enabled_(enabled) {}
	
	ScopeTimer::~ScopeTimer() {
		if (!enabled_) return;
		
		std::fprintf(stderr, "[TIMER] %s: %.3f ms\n", name_, t_.ms());
	}

} 
