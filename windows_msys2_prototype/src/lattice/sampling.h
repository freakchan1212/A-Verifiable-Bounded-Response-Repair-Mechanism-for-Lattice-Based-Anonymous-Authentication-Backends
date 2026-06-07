
#pragma once
#include "common/params.h"
#include "common/rng.h"
#include "mod_arith.h"

namespace lattice {

	
	inline int sample_small(const common::Params& p, common::Rng& rng) {
		return rng.small_int(p.eta);
	}
	
	inline int sample_uniform_q(const common::Params& p, common::Rng& rng) {
		return rng.uniform_int(0, p.q - 1);
	}

} 
