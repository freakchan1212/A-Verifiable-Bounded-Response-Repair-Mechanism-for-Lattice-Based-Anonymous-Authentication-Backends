
#pragma once
#include <cstdint>
#include <vector>

#include "common/params.h"
#include "lattice/polyvec.h"
#include "preauth/repair.h"

namespace zk {

	
	
	
	
	
	
	
	struct BoundaryRangeProof {
		uint32_t L = 0;                   
		std::vector<uint32_t> idx;        
		std::vector<uint8_t> u_bits;      
	};
	
	uint64_t DigestBoundaryRangeProof(const BoundaryRangeProof& pf);
	
	
	
	BoundaryRangeProof ProveBoundaryRange(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_star,
		const lattice::PolyVec& z,
		uint32_t L_bits);
	
	
	bool VerifyBoundaryRange(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z,
		const BoundaryRangeProof& pf,
		std::vector<int>& out_x_centered);
	
	
	
	
	
	lattice::PolyVec BuildY3_FromZ_AndBoundaryX(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z,
		const std::vector<uint32_t>& bidx,
		const std::vector<int>& x_centered);

} 
