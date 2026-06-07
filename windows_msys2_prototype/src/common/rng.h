
#pragma once
#include <cstdint>

namespace common {

	
	
	class Rng {
	public:
		explicit Rng(uint64_t seed = 0x12345678ULL);
	
		uint64_t next_u64();
		uint32_t next_u32();
		uint8_t  next_u8();
	
		
		int uniform_int(int lo, int hi);
	
		
		int bit();
	
		
		int small_int(int eta);
	
	private:
		void refill();
	
		uint64_t seed_;
		uint64_t block_ctr_;
		uint8_t  buf_[136];
		int      buf_pos_;
		int      buf_len_;
	};

} 
