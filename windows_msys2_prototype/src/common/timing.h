


#pragma once
#include <cstdint>
#include <chrono>
#include <string>

namespace common {

	
	class Timer {
	public:
		Timer();
	
		void reset();
		int64_t us() const; 
		double ms() const;  
	
	private:
		std::chrono::high_resolution_clock::time_point start_;
	};
	
	
	class ScopeTimer {
	public:
		ScopeTimer(const char* name, bool enabled = true);
		~ScopeTimer();
	
	private:
		const char* name_;
		bool enabled_;
		Timer t_;
	};

} 
