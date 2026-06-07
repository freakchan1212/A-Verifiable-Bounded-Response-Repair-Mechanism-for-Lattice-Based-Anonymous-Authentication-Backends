
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace common::hash {

	
	std::vector<uint8_t> shake256(const std::vector<uint8_t>& data, size_t outlen);
	
	
	uint64_t hash64(const std::vector<uint8_t>& data);
	
	
	std::vector<uint8_t> hash32(const std::vector<uint8_t>& data);

} 
