
#include "poly.h"
#include "mod_arith.h"
#include "sampling.h"

#include <sstream>

namespace lattice {

	Poly Poly::zero(const common::Params& p) {
		return Poly(p.N);
	}
	
	Poly Poly::uniform(const common::Params& p, common::Rng& rng) {
		Poly a(p.N);
		for (int i = 0; i < p.N; i++) {
			a.c[(size_t)i] = (coeff_t)sample_uniform_q(p, rng);
		}
		return a;
	}
	
	Poly Poly::small(const common::Params& p, common::Rng& rng) {
		Poly a(p.N);
		for (int i = 0; i < p.N; i++) {
			a.c[(size_t)i] = (coeff_t)sample_small(p, rng);
		}
		return a;
	}
	
	std::string Poly::to_string(int max_terms) const {
		std::ostringstream oss;
		oss << "[";
		int n = (int)c.size();
		int lim = (max_terms < n ? max_terms : n);
		for (int i = 0; i < lim; i++) {
			oss << (int)c[(size_t)i];
			if (i + 1 < lim) oss << " ";
		}
		if (lim < n) oss << " ...";
		oss << "]";
		return oss.str();
	}
	
	Poly add(const common::Params& p, const Poly& a, const Poly& b) {
		Poly r(p.N);
		for (int i = 0; i < p.N; i++) {
			int x = (int)a.c[(size_t)i] + (int)b.c[(size_t)i];
			r.c[(size_t)i] = (Poly::coeff_t)modq(x, p.q);
		}
		return r;
	}
	
	Poly sub(const common::Params& p, const Poly& a, const Poly& b) {
		Poly r(p.N);
		for (int i = 0; i < p.N; i++) {
			int x = (int)a.c[(size_t)i] - (int)b.c[(size_t)i];
			r.c[(size_t)i] = (Poly::coeff_t)modq(x, p.q);
		}
		return r;
	}
	
	Poly mul_schoolbook(const common::Params& p, const Poly& a, const Poly& b) {
		Poly r(p.N);
	
		for (int i = 0; i < p.N; i++) {
			for (int j = 0; j < p.N; j++) {
				long long prod = 1LL * (int)a.c[(size_t)i] * (int)b.c[(size_t)j];
				int idx = i + j;
	
				if (idx < p.N) {
					int cur = (int)r.c[(size_t)idx];
					r.c[(size_t)idx] = (Poly::coeff_t)modq(cur + (int)prod, p.q);
				}
				else {
					int ridx = idx - p.N;
					int cur = (int)r.c[(size_t)ridx];
					r.c[(size_t)ridx] = (Poly::coeff_t)modq(cur - (int)prod, p.q);
				}
			}
		}
		return r;
	}
	
	Poly mod_reduce(const common::Params& p, const Poly& a) {
		Poly r(p.N);
		for (int i = 0; i < p.N; i++) {
			r.c[(size_t)i] = (Poly::coeff_t)modq((int)a.c[(size_t)i], p.q);
		}
		return r;
	}
	
	Poly center_reduce(const common::Params& p, const Poly& a) {
		Poly r(p.N);
		for (int i = 0; i < p.N; i++) {
			r.c[(size_t)i] = (Poly::coeff_t)center_q((int)a.c[(size_t)i], p.q);
		}
		return r;
	}

} 
