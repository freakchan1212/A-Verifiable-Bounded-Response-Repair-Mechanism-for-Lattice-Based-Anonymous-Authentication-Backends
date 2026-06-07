
#pragma once
#include <vector>
#include <cstdint>
#include "common/params.h"
#include "lattice/polyvec.h"

namespace preauth {

	
	std::vector<uint8_t> commit_zstar(const common::Params& p, const lattice::PolyVec& z_star);

} 
