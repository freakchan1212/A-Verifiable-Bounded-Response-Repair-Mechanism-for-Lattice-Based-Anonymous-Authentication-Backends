
#pragma once
#include "common/params.h"
#include "poly.h"

namespace lattice::kyber_ref {

	bool supported(const common::Params& p);
	
	
	void poly_ntt(const common::Params& p, Poly& a);
	void poly_invntt_tomont(const common::Params& p, Poly& a);
	void poly_basemul_montgomery(const common::Params& p, Poly& r, const Poly& a, const Poly& b);
	
	
	Poly mul_poly(const common::Params& p, const Poly& a, const Poly& b);

} 
