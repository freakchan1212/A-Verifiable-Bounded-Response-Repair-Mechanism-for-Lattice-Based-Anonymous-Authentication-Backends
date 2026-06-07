
#pragma once
#include <cstdint>
#include <vector>

#include "common/params.h"
#include "common/rng.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"


namespace zk {

	struct JointSigmaProof {
		lattice::PolyVec a1;
		lattice::PolyVec a2;
		lattice::PolyVec s_z;
		lattice::PolyVec s_r;
	};
	
	JointSigmaProof ProveJoint(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyMat& Acom,
		const lattice::PolyVec& y,
		const lattice::PolyVec& C,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const lattice::PolyVec& z_witness_modq,
		const lattice::PolyVec& r_witness_modq,
		common::Rng& rng);
	
	bool VerifyJoint(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyMat& Acom,
		const lattice::PolyVec& y,
		const lattice::PolyVec& C,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const JointSigmaProof& proof);

} 
