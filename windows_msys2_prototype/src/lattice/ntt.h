
#pragma once
#include "common/params.h"
#include "poly.h"

namespace lattice::ntt {

	
	
	
	bool supported(const common::Params& p);
	
	
	
	
	void forward(const common::Params& p, Poly& a);
	void inverse(const common::Params& p, Poly& a);
	
	
	
	
	void mul_freq(const common::Params& p, const Poly& A, const Poly& B, Poly& C);
	
	
	Poly mul_ntt(const common::Params& p, const Poly& a, const Poly& b);

} 
