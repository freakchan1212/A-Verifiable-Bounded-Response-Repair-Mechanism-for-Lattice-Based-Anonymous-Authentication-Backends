
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace common {

	
	
	inline int mod(int x, int m) {
		int r = x % m;     
		if (r < 0) r += m; 
		return r;
	}
	
	
	
	
	
	
	
	inline int center(int x, int m) {
		int r = mod(x, m);
		int half = m / 2;
		if (r > half) r -= m;
		return r;
	}
	
	
	
	template <typename T>
	inline T clamp(T v, T lo, T hi) {
		return std::min(std::max(v, lo), hi);
	}
	
	
	
	
	inline std::string hex_u64(uint64_t x) {
		std::ostringstream oss;
		oss << "0x" << std::hex << std::setw(16) << std::setfill('0') << x;
		return oss.str();
	}
	
	
	
	
	template <typename T>
	inline std::string join(const std::vector<T>& a, const char* sep = " ") {
		std::ostringstream oss;
		for (size_t i = 0; i < a.size(); i++) {
			oss << a[i];
			if (i + 1 < a.size()) oss << sep;
		}
		return oss.str();
	}

} 
