
#include "module_lwe.h"

namespace lattice {

	
	
	
	
	
	
	KeyPair keygen(const common::Params& p, common::Rng& rng) {
		KeyPair kp;
	
		
		kp.pk.A = mat_uniform(p, rng);
	
		
		kp.sk.s = vec_small(p, rng);
	
		
		kp.sk.e = vec_small(p, rng);
	
		
		PolyVec As = mat_vec_mul(p, kp.pk.A, kp.sk.s);
	
		
		
		kp.pk.t = vec_add(p, As, kp.sk.e);
	
		return kp;
	}

} 
