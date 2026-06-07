
#include "smoothing.h"
#include "lattice/polyvec.h"

namespace preauth {

	lattice::PolyVec sample_smoothing_noise(const common::Params& p, common::Rng& rng) {
		lattice::PolyVec u = lattice::vec_zero(p);
		const int eta = (p.smoothing_eta <= 0 ? 1 : p.smoothing_eta);
	
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				u.v[i].c[j] = rng.small_int(eta);
			}
		}
		return u;
	}

} 
