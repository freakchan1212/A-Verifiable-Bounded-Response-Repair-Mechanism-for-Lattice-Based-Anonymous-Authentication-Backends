#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

#include "common/params.h"
#include "lattice/polyvec.h"

namespace preauth {

	
	struct RepairTheta {
		int B; 
		int d; 
	};
	
	
	
	
	
	
	
	
	
	
	
	
	
	struct RepairMeta {
		std::vector<int16_t> high; 
		std::vector<int16_t> low;  
		int clamp_count = 0;
		
		
		int delta_l0 = 0;
		int delta_linf = 0;
		std::vector<uint8_t> delta_digest; 
		
		
		std::vector<uint8_t> ctx_digest;   
		uint64_t refresh_tag = 0;
		std::vector<uint8_t> repair_digest; 
	};
	
	struct RepairResult {
		lattice::PolyVec z;     
		lattice::PolyVec delta; 
		RepairMeta meta;
		
		bool repaired = false;
		int delta_l0 = 0;
		int delta_linf = 0;
		std::vector<uint8_t> delta_digest;
	};
	
	RepairResult repair_polyvec(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z_star);
	
	
	
	bool check_repair(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z,
		const RepairMeta& meta);
	
	
	
	
	bool check_repair_delta(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z,
		const lattice::PolyVec& delta,
		const RepairMeta& meta);
	
	
	
	bool check_repair_context_binding(const RepairMeta& meta,
		const std::vector<uint8_t>& expected_ctx_digest,
		uint64_t expected_refresh_tag);
	
	
	
	void bind_repair_meta(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z,
		const lattice::PolyVec& delta,
		const std::vector<uint8_t>& ctx_digest,
		uint64_t refresh_tag,
		RepairMeta& meta);
	
	
	lattice::PolyVec recover_zstar_from_repair(const common::Params& p,
		const lattice::PolyVec& z,
		const lattice::PolyVec& delta);
	
	int polyvec_l0_centered(const common::Params& p, const lattice::PolyVec& v);
	int polyvec_linf_centered(const common::Params& p, const lattice::PolyVec& v);
	std::vector<uint8_t> digest_polyvec_centered(const common::Params& p, const lattice::PolyVec& v);
	size_t repair_meta_payload_bytes(const RepairMeta& meta);

} 
