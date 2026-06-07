
#pragma once
#include <vector>

#include "common/params.h"
#include "common/rng.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"

#include "offload/adversary.h"

namespace offload {

	struct OffloadStats {
		int dim_k = 0;      
	
		int k = 0;          
		int r = 0;
		int n = 0;
	
		int received = 0;
		int used = 0;
		int dropped = 0;
		int corrupted = 0;
	
		int waited = 0;
		int tried = 0;
	
		bool recovered = false;
		bool freivalds_ok = false;
		bool fallback_local = false;
	};
	
	bool CodedOffloadMatVec(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyVec& x,
		lattice::PolyVec& y_out,
		OffloadStats& st,
		common::Rng& rng,
		const AdversaryConfig& adv);

} 
