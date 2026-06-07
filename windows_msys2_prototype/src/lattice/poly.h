
#pragma once
#include <vector>
#include <string>
#include <cstdint>

#include "common/params.h"
#include "common/rng.h"

namespace lattice {

	struct Poly {
		using coeff_t = int16_t;
	
		
		std::vector<coeff_t> c;
	
		Poly() = default;
		explicit Poly(int N) : c((size_t)N, (coeff_t)0) {}
	
		static Poly zero(const common::Params& p);
		static Poly uniform(const common::Params& p, common::Rng& rng); 
		static Poly small(const common::Params& p, common::Rng& rng);   
	
		std::string to_string(int max_terms = 8) const;
	};
	
	
	Poly add(const common::Params& p, const Poly& a, const Poly& b);
	
	
	Poly sub(const common::Params& p, const Poly& a, const Poly& b);
	
	
	Poly mul_schoolbook(const common::Params& p, const Poly& a, const Poly& b);
	
	
	Poly mod_reduce(const common::Params& p, const Poly& a);
	
	
	Poly center_reduce(const common::Params& p, const Poly& a);

} 
