
#pragma once
#include <cstdint>
#include <vector>

#include "common/params.h"
#include "common/rng.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"


namespace zk {

	struct LinearSigmaProof {
		lattice::PolyVec a; 
		lattice::PolyVec s; 
	};
	
	LinearSigmaProof Prove(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyVec& y,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const lattice::PolyVec& z_witness_modq,
		common::Rng& rng);
	
	bool Verify(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyVec& y,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const LinearSigmaProof& proof);

} 
