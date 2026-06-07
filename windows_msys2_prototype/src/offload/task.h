
#pragma once
#include <vector>

#include "common/params.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"
#include "lattice/poly.h"

namespace offload {

	
	
	struct EncodedRowTask {
		int shard_id = 0;
		int eval = 1;                    
		std::vector<int> coeff_row;      
		lattice::PolyVec rowcomb;        
	};
	
	EncodedRowTask MakeEncodedRowTask(const common::Params& p,
		const lattice::PolyMat& A,
		int shard_id,
		int eval);
	
	lattice::Poly RunEncodedRowTask(const common::Params& p,
		const EncodedRowTask& task,
		const lattice::PolyVec& x);

} 
