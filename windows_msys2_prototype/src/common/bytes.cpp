
#include "bytes.h"

namespace common::bytes {

	
	
	
	
	
	
	void append_u16(std::vector<uint8_t>& out, uint16_t x) {
		out.push_back((uint8_t)(x & 0xFF));
		out.push_back((uint8_t)((x >> 8) & 0xFF));
	}
	
	void append_u32(std::vector<uint8_t>& out, uint32_t x) {
		out.push_back((uint8_t)(x & 0xFF));
		out.push_back((uint8_t)((x >> 8) & 0xFF));
		out.push_back((uint8_t)((x >> 16) & 0xFF));
		out.push_back((uint8_t)((x >> 24) & 0xFF));
	}
	
	void append_u64(std::vector<uint8_t>& out, uint64_t x) {
		
		for (int i = 0; i < 8; i++) {
			out.push_back((uint8_t)((x >> (8 * i)) & 0xFF));
		}
	}
	
	
	
	void append_i16(std::vector<uint8_t>& out, int16_t x) {
		append_u16(out, (uint16_t)x);
	}
	
	
	void append_bytes(std::vector<uint8_t>& out, const std::vector<uint8_t>& b) {
		out.insert(out.end(), b.begin(), b.end());
	}
	
	
	
	
	
	
	
	void append_str(std::vector<uint8_t>& out, const std::string& s) {
		out.insert(out.end(), s.begin(), s.end());
		out.push_back(0); 
	}
	
	
	
	
	
	
	
	static bool need(const std::vector<uint8_t>& in, size_t off, size_t n) {
		return off + n <= in.size();
	}
	
	bool read_u16(const std::vector<uint8_t>& in, size_t& off, uint16_t& x) {
		if (!need(in, off, 2)) return false;
		x = (uint16_t)in[off] | ((uint16_t)in[off + 1] << 8);
		off += 2;
		return true;
	}
	
	bool read_u32(const std::vector<uint8_t>& in, size_t& off, uint32_t& x) {
		if (!need(in, off, 4)) return false;
		x = (uint32_t)in[off]
			| ((uint32_t)in[off + 1] << 8)
			| ((uint32_t)in[off + 2] << 16)
			| ((uint32_t)in[off + 3] << 24);
		off += 4;
		return true;
	}
	
	bool read_u64(const std::vector<uint8_t>& in, size_t& off, uint64_t& x) {
		if (!need(in, off, 8)) return false;
		x = 0;
		for (int i = 0; i < 8; i++) {
			x |= ((uint64_t)in[off + i] << (8 * i));
		}
		off += 8;
		return true;
	}
	
	bool read_i16(const std::vector<uint8_t>& in, size_t& off, int16_t& x) {
		uint16_t t = 0;
		if (!read_u16(in, off, t)) return false;
		x = (int16_t)t;
		return true;
	}

} 
