
#include "hash.h"
#include "bytes.h"

#include <cstring>
#include <vector>
#include <cstdint>

namespace {

	static inline uint64_t rol64(uint64_t x, int s) {
		return (s == 0) ? x : ((x << s) | (x >> (64 - s)));
	}
	
	static void keccakf(uint64_t st[25]) {
		static const uint64_t RC[24] = {
			0x0000000000000001ULL, 0x0000000000008082ULL,
			0x800000000000808aULL, 0x8000000080008000ULL,
			0x000000000000808bULL, 0x0000000080000001ULL,
			0x8000000080008081ULL, 0x8000000000008009ULL,
			0x000000000000008aULL, 0x0000000000000088ULL,
			0x0000000080008009ULL, 0x000000008000000aULL,
			0x000000008000808bULL, 0x800000000000008bULL,
			0x8000000000008089ULL, 0x8000000000008003ULL,
			0x8000000000008002ULL, 0x8000000000000080ULL,
			0x000000000000800aULL, 0x800000008000000aULL,
			0x8000000080008081ULL, 0x8000000000008080ULL,
			0x0000000080000001ULL, 0x8000000080008008ULL
		};
	
		static const int rotc[24] = {
			1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
			27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
		};
	
		static const int piln[24] = {
			10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
			15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
		};
	
		for (int round = 0; round < 24; round++) {
			uint64_t bc[5], t;
	
			for (int i = 0; i < 5; i++) {
				bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
			}
			for (int i = 0; i < 5; i++) {
				t = bc[(i + 4) % 5] ^ rol64(bc[(i + 1) % 5], 1);
				for (int j = 0; j < 25; j += 5) st[j + i] ^= t;
			}
	
			t = st[1];
			for (int i = 0; i < 24; i++) {
				int j = piln[i];
				bc[0] = st[j];
				st[j] = rol64(t, rotc[i]);
				t = bc[0];
			}
	
			for (int j = 0; j < 25; j += 5) {
				for (int i = 0; i < 5; i++) bc[i] = st[j + i];
				for (int i = 0; i < 5; i++) st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
			}
	
			st[0] ^= RC[round];
		}
	}
	
	class Shake256 {
	public:
		Shake256() { reset(); }
	
		void reset() {
			std::memset(st_, 0, sizeof(st_));
			pos_ = 0;
			finalized_ = false;
		}
	
		void absorb(const uint8_t* in, size_t inlen) {
			const size_t rate = 136; 
			if (finalized_) return;
	
			uint8_t* sb = reinterpret_cast<uint8_t*>(st_);
			while (inlen > 0) {
				size_t take = rate - pos_;
				if (take > inlen) take = inlen;
	
				for (size_t i = 0; i < take; i++) sb[pos_ + i] ^= in[i];
	
				pos_ += take;
				in += take;
				inlen -= take;
	
				if (pos_ == rate) {
					keccakf(st_);
					pos_ = 0;
				}
			}
		}
	
		void finalize() {
			const size_t rate = 136;
			uint8_t* sb = reinterpret_cast<uint8_t*>(st_);
			sb[pos_] ^= 0x1F;
			sb[rate - 1] ^= 0x80;
			keccakf(st_);
			pos_ = 0;
			finalized_ = true;
		}
	
		void squeeze(uint8_t* out, size_t outlen) {
			const size_t rate = 136;
			if (!finalized_) finalize();
	
			uint8_t* sb = reinterpret_cast<uint8_t*>(st_);
			while (outlen > 0) {
				size_t take = rate - pos_;
				if (take > outlen) take = outlen;
	
				std::memcpy(out, sb + pos_, take);
				pos_ += take;
				out += take;
				outlen -= take;
	
				if (pos_ == rate) {
					keccakf(st_);
					pos_ = 0;
				}
			}
		}
	
	private:
		uint64_t st_[25];
		size_t pos_;
		bool finalized_;
	};
	
	static std::vector<uint8_t> shake256_impl(const std::vector<uint8_t>& data, size_t outlen) {
		Shake256 sh;
		if (!data.empty()) sh.absorb(data.data(), data.size());
		std::vector<uint8_t> out(outlen, 0);
		if (outlen > 0) sh.squeeze(out.data(), out.size());
		return out;
	}

} 

namespace common::hash {

	std::vector<uint8_t> shake256(const std::vector<uint8_t>& data, size_t outlen) {
		return shake256_impl(data, outlen);
	}
	
	uint64_t hash64(const std::vector<uint8_t>& data) {
		std::vector<uint8_t> in;
		common::bytes::append_str(in, "common.hash64.shake256.v1");
		common::bytes::append_bytes(in, data);
	
		std::vector<uint8_t> out = shake256_impl(in, 8);
		uint64_t x = 0;
		for (int i = 0; i < 8; i++) x |= ((uint64_t)out[(size_t)i] << (8 * i));
		return x;
	}
	
	std::vector<uint8_t> hash32(const std::vector<uint8_t>& data) {
		std::vector<uint8_t> in;
		common::bytes::append_str(in, "common.hash32.shake256.v1");
		common::bytes::append_bytes(in, data);
		return shake256_impl(in, 32);
	}

} 
