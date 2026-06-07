
#pragma once
#include <vector>
#include <string>

#include "common/params.h"
#include "common/rng.h"
#include "poly.h"
#include "polyvec.h"

namespace lattice {

	struct PolyMat {
		
		std::vector<std::vector<Poly>> m;
	
		PolyMat() = default;
		explicit PolyMat(int k, int N) : m((size_t)k, std::vector<Poly>((size_t)k, Poly(N))) {}
	
		std::string to_string(int max_terms_per_poly = 4) const;
	};
	
	PolyMat mat_uniform(const common::Params& p, common::Rng& rng);
	
	
	
	
	
	
	PolyVec mat_vec_mul(const common::Params& p, const PolyMat& A, const PolyVec& x);

} 
