
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace preauth {

	class Transcript {
	public:
		void reset();
	
		void append_tag(const std::string& s);
		void append_u32(uint32_t x);
		void append_u64(uint64_t x);
		void append_bytes(const std::vector<uint8_t>& b);
	
		
		std::vector<int> challenge_bits(int out_len) const;
	
		
		uint64_t challenge_seed() const;
	
	private:
		std::vector<uint8_t> buf_;
	};

} 
