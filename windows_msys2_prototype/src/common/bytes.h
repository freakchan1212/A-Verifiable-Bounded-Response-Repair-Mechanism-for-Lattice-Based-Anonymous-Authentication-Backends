
#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace common::bytes {

	
	void append_u16(std::vector<uint8_t>& out, uint16_t x);
	void append_u32(std::vector<uint8_t>& out, uint32_t x);
	void append_u64(std::vector<uint8_t>& out, uint64_t x);
	void append_i16(std::vector<uint8_t>& out, int16_t x);
	void append_bytes(std::vector<uint8_t>& out, const std::vector<uint8_t>& b);
	void append_str(std::vector<uint8_t>& out, const std::string& s);
	
	
	
	bool read_u16(const std::vector<uint8_t>& in, size_t& off, uint16_t& x);
	bool read_u32(const std::vector<uint8_t>& in, size_t& off, uint32_t& x);
	bool read_u64(const std::vector<uint8_t>& in, size_t& off, uint64_t& x);
	bool read_i16(const std::vector<uint8_t>& in, size_t& off, int16_t& x);

} 
