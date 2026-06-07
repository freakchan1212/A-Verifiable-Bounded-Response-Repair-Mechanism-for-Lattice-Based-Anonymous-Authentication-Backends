
#pragma once
#include <vector>
#include <string>
#include "common/params.h"
#include "common/rng.h"
#include "poly.h"

namespace lattice {

	struct PolyVec {
		std::vector<Poly> v; 
	
		PolyVec() = default;
		explicit PolyVec(int k, int N) : v(k, Poly(N)) {}
	
		std::string to_string(int max_terms_per_poly = 6) const;
	};
	
	PolyVec vec_zero(const common::Params& p);
	PolyVec vec_uniform(const common::Params& p, common::Rng& rng);
	PolyVec vec_small(const common::Params& p, common::Rng& rng);
	
	PolyVec vec_add(const common::Params& p, const PolyVec& a, const PolyVec& b);
	PolyVec vec_sub(const common::Params& p, const PolyVec& a, const PolyVec& b);
	
	
	PolyVec vec_mod_reduce(const common::Params& p, const PolyVec& a);
	
	PolyVec vec_center_reduce(const common::Params& p, const PolyVec& a);

} 
