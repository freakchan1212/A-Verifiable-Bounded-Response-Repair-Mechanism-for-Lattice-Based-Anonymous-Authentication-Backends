
#include "offload/freivalds.h"
#include "lattice/poly.h"
#include "lattice/mod_arith.h"
#include "lattice/ntt.h"

namespace offload {

	static lattice::Poly poly_zero(const common::Params& p) { return lattice::Poly::zero(p); }
	
	static lattice::Poly poly_scalar_mul(const common::Params& p, const lattice::Poly& a, int s) {
		lattice::Poly r(p.N);
		for (int i = 0; i < p.N; i++) {
			long long v = 1LL * lattice::modq(a.c[i], p.q) * lattice::modq(s, p.q);
			r.c[i] = lattice::modq((int)v, p.q);
		}
		return r;
	}
	
	static bool poly_equal_modq(const common::Params& p, const lattice::Poly& a, const lattice::Poly& b) {
		for (int i = 0; i < p.N; i++) {
			if (lattice::modq(a.c[i], p.q) != lattice::modq(b.c[i], p.q)) return false;
		}
		return true;
	}
	
	bool CheckMatVecFreivalds(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyVec& x,
		const lattice::PolyVec& y,
		int rounds,
		common::Rng& rng) {
		if (rounds <= 0) return true;
	
		for (int t = 0; t < rounds; t++) {
			std::vector<int> alpha(p.k, 0);
			for (int i = 0; i < p.k; i++) alpha[i] = rng.uniform_int(0, p.q - 1);
	
			lattice::Poly lhs = poly_zero(p);
			for (int i = 0; i < p.k; i++) {
				lhs = lattice::add(p, lhs, poly_scalar_mul(p, y.v[i], alpha[i]));
			}
	
			lattice::PolyVec rowcomb = lattice::vec_zero(p);
			for (int j = 0; j < p.k; j++) {
				lattice::Poly acc = poly_zero(p);
				for (int i = 0; i < p.k; i++) {
					acc = lattice::add(p, acc, poly_scalar_mul(p, A.m[i][j], alpha[i]));
				}
				rowcomb.v[j] = acc;
			}
	
			lattice::Poly rhs = poly_zero(p);
			for (int j = 0; j < p.k; j++) {
				lattice::Poly prod = lattice::ntt::mul_ntt(p, rowcomb.v[j], x.v[j]);
				rhs = lattice::add(p, rhs, prod);
			}
	
			if (!poly_equal_modq(p, lhs, rhs)) return false;
		}
	
		return true;
	}

} 
