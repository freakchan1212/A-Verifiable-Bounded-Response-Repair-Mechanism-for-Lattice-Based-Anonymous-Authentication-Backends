


#include "rng.h"
#include "hash.h"
#include "bytes.h"

#include <vector>
#include <cstdint>
#include <cstring>
#include <limits>

namespace common {

	Rng::Rng(uint64_t seed)
		: seed_(seed), block_ctr_(0), buf_pos_(0), buf_len_(0) {
		std::memset(buf_, 0, sizeof(buf_));
	}
	
	void Rng::refill() {
		std::vector<uint8_t> in;
		common::bytes::append_str(in, "common.rng.shake256drbg.v1");
		common::bytes::append_u64(in, seed_);
		common::bytes::append_u64(in, block_ctr_++);
	
		std::vector<uint8_t> out = common::hash::shake256(in, sizeof(buf_));
		for (size_t i = 0; i < out.size(); i++) buf_[i] = out[i];
	
		buf_pos_ = 0;
		buf_len_ = (int)out.size();
	}
	
	uint8_t Rng::next_u8() {
		if (buf_pos_ >= buf_len_) refill();
		return buf_[buf_pos_++];
	}
	
	uint32_t Rng::next_u32() {
		uint32_t x = 0;
		for (int i = 0; i < 4; i++) {
			x |= ((uint32_t)next_u8() << (8 * i));
		}
		return x;
	}
	
	uint64_t Rng::next_u64() {
		uint64_t x = 0;
		for (int i = 0; i < 8; i++) {
			x |= ((uint64_t)next_u8() << (8 * i));
		}
		return x;
	}
	
	int Rng::uniform_int(int lo, int hi) {
		if (hi < lo) {
			int t = lo;
			lo = hi;
			hi = t;
		}
		if (lo == hi) return lo;
	
		const uint64_t span64 = (uint64_t)((int64_t)hi - (int64_t)lo) + 1ULL;
		if (span64 == 0ULL || span64 > 0xFFFFFFFFULL) {
			return lo;
		}
	
		const uint32_t span = (uint32_t)span64;
		const uint32_t maxv = std::numeric_limits<uint32_t>::max();
		const uint32_t lim = maxv - (maxv % span);
	
		uint32_t r = 0;
		do {
			r = next_u32();
		} while (r > lim);
	
		return lo + (int)(r % span);
	}
	
	int Rng::bit() {
		return (int)(next_u8() & 1u);
	}
	
	int Rng::small_int(int eta) {
		if (eta <= 0) return 0;
	
		int a = 0, b = 0;
		for (int i = 0; i < eta; i++) {
			a += bit();
			b += bit();
		}
		return a - b;
	}

} 
