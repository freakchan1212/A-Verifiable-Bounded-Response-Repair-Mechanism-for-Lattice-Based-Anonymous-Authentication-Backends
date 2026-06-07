
#pragma once
#include "common/params.h"
#include "common/rng.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"

namespace offload {

	
	
	bool CheckMatVecFreivalds(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyVec& x,
		const lattice::PolyVec& y,
		int rounds,
		common::Rng& rng);

} 
