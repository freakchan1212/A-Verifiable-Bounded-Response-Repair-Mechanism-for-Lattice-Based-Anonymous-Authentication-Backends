
#pragma once
#include <cstdint>
#include <vector>

#include "common/params.h"
#include "common/rng.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"


namespace zk {

	struct BoundJointSigmaProof {
		lattice::PolyVec a1;
		lattice::PolyVec a2;
		lattice::PolyVec a3;  
		lattice::PolyVec s_z;
		lattice::PolyVec s_r;
	};
	
	BoundJointSigmaProof ProveBoundJoint(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyMat& Acom,
		const lattice::PolyVec& y,
		const lattice::PolyVec& C,
		const lattice::PolyVec& z_pub_modq,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const lattice::PolyVec& z_wit_modq,
		const lattice::PolyVec& r_wit_modq,
		common::Rng& rng);
	
	bool VerifyBoundJoint(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyMat& Acom,
		const lattice::PolyVec& y,
		const lattice::PolyVec& C,
		const lattice::PolyVec& z_pub_modq,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const BoundJointSigmaProof& pf);

} 
