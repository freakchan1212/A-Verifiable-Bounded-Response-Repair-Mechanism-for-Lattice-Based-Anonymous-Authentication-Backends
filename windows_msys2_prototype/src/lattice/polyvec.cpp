
#include "polyvec.h"
#include <sstream>

namespace lattice {

	std::string PolyVec::to_string(int max_terms_per_poly) const {
		std::ostringstream oss;
		oss << "{";
		for (size_t i = 0; i < v.size(); i++) {
			oss << v[i].to_string(max_terms_per_poly);
			if (i + 1 < v.size()) oss << ", ";
		}
		oss << "}";
		return oss.str();
	}
	
	PolyVec vec_zero(const common::Params& p) {
		PolyVec z;
		z.v.resize(p.k);
		for (int i = 0; i < p.k; i++) z.v[i] = Poly::zero(p);
		return z;
	}
	
	PolyVec vec_uniform(const common::Params& p, common::Rng& rng) {
		PolyVec z;
		z.v.resize(p.k);
		for (int i = 0; i < p.k; i++) z.v[i] = Poly::uniform(p, rng);
		return z;
	}
	
	PolyVec vec_small(const common::Params& p, common::Rng& rng) {
		PolyVec z;
		z.v.resize(p.k);
		for (int i = 0; i < p.k; i++) z.v[i] = Poly::small(p, rng);
		return z;
	}
	
	PolyVec vec_add(const common::Params& p, const PolyVec& a, const PolyVec& b) {
		PolyVec r;
		r.v.resize(p.k);
		for (int i = 0; i < p.k; i++) r.v[i] = add(p, a.v[i], b.v[i]);
		return r;
	}
	
	PolyVec vec_sub(const common::Params& p, const PolyVec& a, const PolyVec& b) {
		PolyVec r;
		r.v.resize(p.k);
		for (int i = 0; i < p.k; i++) r.v[i] = sub(p, a.v[i], b.v[i]);
		return r;
	}
	
	PolyVec vec_mod_reduce(const common::Params& p, const PolyVec& a) {
		PolyVec r;
		r.v.resize(p.k);
		for (int i = 0; i < p.k; i++) r.v[i] = mod_reduce(p, a.v[i]);
		return r;
	}
	
	PolyVec vec_center_reduce(const common::Params& p, const PolyVec& a) {
		PolyVec r;
		r.v.resize(p.k);
		for (int i = 0; i < p.k; i++) r.v[i] = center_reduce(p, a.v[i]);
		return r;
	}

} 
