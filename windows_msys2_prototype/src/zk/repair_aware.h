
#pragma once
#include <vector>
#include <cstdint>

#include "common/params.h"
#include "lattice/polyvec.h"
#include "preauth/repair.h"

namespace zk {

	
	
	
	
	struct RepairAwareProof {
		std::vector<uint8_t> com;        
		lattice::PolyVec z_star_open;    
	};
	
	
	
	RepairAwareProof ProveRepair(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_star,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta);
	
	
	
	bool VerifyRepair(const common::Params& p,
		const preauth::RepairTheta& th,
		const RepairAwareProof& pf,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta);

} 
