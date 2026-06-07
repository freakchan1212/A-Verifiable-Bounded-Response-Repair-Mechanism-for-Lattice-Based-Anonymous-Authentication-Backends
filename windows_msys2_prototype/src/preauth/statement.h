
#pragma once
#include "common/params.h"
#include "lattice/module_lwe.h"
#include "preauth/commit.h"
#include "preauth/repair.h"

namespace preauth::stmt {

	
	
	bool CheckRepairRelation(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z_star,
		const lattice::PolyVec& z,
		const RepairMeta& meta);
	
	
	
	bool CheckResidualSmallCentered(const common::Params& p,
		const lattice::PolyVec& r_small);
	
	
	lattice::PolyVec BuildYFromCommit(const common::Params& p,
		const lattice::PublicKey& pk,
		const CommitMsg& cm,
		int c,
		const lattice::PolyVec& r_small);

} 
