
#pragma once
#include "common/params.h"
#include "lattice/module_lwe.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"

namespace zk {

	
	
	
	
	
	
	
	
	
	
	lattice::PolyMat DeriveCommitKey(const common::Params& p,
		const lattice::PublicKey& pk);
	
	
	
	
	
	lattice::PolyVec Commit(const common::Params& p,
		const lattice::PolyMat& Acom,
		const lattice::PolyVec& r_modq,
		const lattice::PolyVec& z_modq);

} 
