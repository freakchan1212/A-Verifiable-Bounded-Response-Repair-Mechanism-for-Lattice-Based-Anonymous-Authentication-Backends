
#pragma once
#include <cstdint>
#include <vector>

#include "common/params.h"
#include "lattice/polyvec.h"
#include "preauth/repair.h"

namespace zk {

	
	
	
	struct BoundaryProof {
		std::vector<uint32_t> b_idx;   
		std::vector<int16_t>  b_val;   
	
		std::vector<uint32_t> a_idx;   
		std::vector<int16_t>  a_val;   
	};
	
	
	uint64_t DigestBoundaryProof(const BoundaryProof& pf);
	
	
	BoundaryProof ProveBoundaryToy(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_star,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta,
		uint32_t nonce,
		const lattice::PolyVec& C,
		int audit_t);
	
	
	bool VerifyBoundaryToy(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta,
		uint32_t nonce,
		const lattice::PolyVec& C,
		const BoundaryProof& pf,
		int audit_t);

} 
