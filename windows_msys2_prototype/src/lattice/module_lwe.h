
#pragma once
#include "common/params.h"
#include "common/rng.h"
#include "polymat.h"
#include "polyvec.h"

namespace lattice {

	struct PublicKey {
		PolyMat A;
		PolyVec t; 
	};
	
	struct SecretKey {
		PolyVec s;
		PolyVec e; 
	};
	
	struct KeyPair {
		PublicKey pk;
		SecretKey sk;
	};
	
	
	KeyPair keygen(const common::Params& p, common::Rng& rng);

} 
