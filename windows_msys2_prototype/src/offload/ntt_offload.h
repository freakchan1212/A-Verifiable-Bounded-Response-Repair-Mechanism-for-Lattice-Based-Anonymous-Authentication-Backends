
#pragma once
#include "common/params.h"
#include "common/rng.h"
#include "lattice/poly.h"

#include "offload/adversary.h"

namespace offload {

	struct NttOffloadStats {
		int n_calls = 0;
		int ok_calls = 0;
		int fallback_calls = 0;
	};
	
	
	
	
	bool PolyMulOffload(const common::Params& p,
		const lattice::Poly& a,
		const lattice::Poly& b,
		lattice::Poly& c,
		common::Rng& rng,
		const AdversaryConfig& adv,
		NttOffloadStats* st);

} 
