
#pragma once
#include "common/params.h"
#include "common/rng.h"
#include "lattice/polyvec.h"

namespace preauth {

	
	
	lattice::PolyVec sample_smoothing_noise(const common::Params& p, common::Rng& rng);

} 
