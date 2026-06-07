
#pragma once
#include <cstdint>
#include <vector>

#include "common/params.h"
#include "common/rng.h"
#include "lattice/module_lwe.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"
#include "preauth/commit.h"
#include "preauth/repair.h"

namespace zk {

	enum ExactnessMode : uint32_t {
		EXACT_NONE = 0,
		EXACT_DIGEST_BINDING = 1,
		EXACT_BACKEND_INTERFACE = 2
	};

	enum ZkBackendKind : uint32_t {
		ZK_BACKEND_LEGACY_EXACT_BLOB = 1,
		ZK_BACKEND_RELATION_V1 = 2
	};

	enum RepairRelationKind : uint32_t {
		REPAIR_RELATION_FULL_V1 = 1
	};
	
	struct RepairSigmaProof {
		uint32_t L = 0;
		uint32_t backend_kind = ZK_BACKEND_RELATION_V1;
		uint32_t relation_kind = REPAIR_RELATION_FULL_V1;
	
		
		uint32_t exact_mode = EXACT_BACKEND_INTERFACE;
		uint32_t exact_u_max = 0;
		std::vector<uint8_t> relation_digest;  
		std::vector<uint8_t> exact_digest;     
		std::vector<uint8_t> exact_proof_blob; 
		uint32_t exact_boundary_count = 0;
		uint32_t exact_interior_count = 0;
	
		
		lattice::PolyVec a1;
		lattice::PolyVec a2;
		lattice::PolyVec a3;
		lattice::PolyVec a4;
	
		
		lattice::PolyVec s_z;
		lattice::PolyVec s_r;
		std::vector<lattice::PolyVec> s_bits;
	};
	
	RepairSigmaProof ProveRepairSigma(const common::Params& p,
		const lattice::PublicKey& pk,
		const preauth::CommitMsg& cm,
		const lattice::PolyVec& C,
		const lattice::PolyVec& r_small,
		const lattice::PolyVec& z_public_centered,
		const preauth::RepairMeta& meta,
		const lattice::PolyVec& zstar_modq,
		const lattice::PolyVec& r_wit_modq,
		const lattice::PolyVec& zstar_centered,
		const preauth::RepairTheta& th,
		uint32_t Lbits,
		int audit_t_unused,
		common::Rng& rng);

} 
