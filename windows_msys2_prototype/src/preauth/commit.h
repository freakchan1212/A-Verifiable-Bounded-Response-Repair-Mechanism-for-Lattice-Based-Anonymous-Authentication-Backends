#pragma once
#include <cstdint>
#include <vector>

#include "common/params.h"
#include "lattice/polyvec.h"

namespace preauth {

	struct CommitMsg {
		lattice::PolyVec w;
		uint32_t nonce = 0;
		
		
		uint64_t scope_tag = 0;
		uint64_t pseudonym = 0;
		
		
		uint64_t refresh_tag = 0;
	};
	
	std::vector<uint8_t> commit_hash(const common::Params& p, const CommitMsg& cm);

} 
