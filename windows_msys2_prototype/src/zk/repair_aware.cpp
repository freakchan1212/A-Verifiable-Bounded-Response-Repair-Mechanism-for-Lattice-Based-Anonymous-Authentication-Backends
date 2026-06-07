
#include "zk/repair_aware.h"

#include "common/bytes.h"
#include "common/hash.h"
#include "lattice/mod_arith.h"
#include "preauth/statement.h"

namespace zk {

	
	static std::vector<uint8_t> serialize_polyvec_i16_centered(const common::Params& p,
		const lattice::PolyVec& v) {
		std::vector<uint8_t> out;
		out.reserve((size_t)p.k * (size_t)p.N * 2);
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = lattice::center_q(v.v[i].c[j], p.q);
				common::bytes::append_i16(out, (int16_t)x);
			}
		}
		return out;
	}
	
	
	static std::vector<uint8_t> serialize_meta_i16(const preauth::RepairMeta& meta) {
		std::vector<uint8_t> out;
		out.reserve(meta.high.size() * 2 + meta.low.size() * 2 + 8);
	
		common::bytes::append_u32(out, (uint32_t)meta.high.size());
		for (size_t i = 0; i < meta.high.size(); i++) {
			common::bytes::append_i16(out, meta.high[i]);
		}
	
		common::bytes::append_u32(out, (uint32_t)meta.low.size());
		for (size_t i = 0; i < meta.low.size(); i++) {
			common::bytes::append_i16(out, meta.low[i]);
		}
	
		common::bytes::append_u32(out, (uint32_t)meta.clamp_count); 
		return out;
	}
	
	
	
	static std::vector<uint8_t> commit_repair_pf(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_star,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "RepairPf-v1");
	
		common::bytes::append_u32(buf, (uint32_t)th.B);
		common::bytes::append_u32(buf, (uint32_t)th.d);
	
		auto zb = serialize_polyvec_i16_centered(p, z);
		auto mb = serialize_meta_i16(meta);
		auto zsb = serialize_polyvec_i16_centered(p, z_star);
	
		common::bytes::append_bytes(buf, zb);
		common::bytes::append_bytes(buf, mb);
		common::bytes::append_bytes(buf, zsb);
	
		return common::hash::hash32(buf);
	}
	
	RepairAwareProof ProveRepair(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_star,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta) {
		RepairAwareProof pf;
		pf.com = commit_repair_pf(p, th, z_star, z, meta);
		pf.z_star_open = z_star; 
		return pf;
	}
	
	bool VerifyRepair(const common::Params& p,
		const preauth::RepairTheta& th,
		const RepairAwareProof& pf,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta) {
		
		std::vector<uint8_t> expected = commit_repair_pf(p, th, pf.z_star_open, z, meta);
		if (expected.size() != pf.com.size()) return false;
		for (size_t i = 0; i < expected.size(); i++) {
			if (expected[i] != pf.com[i]) return false;
		}
	
		
		if (!preauth::stmt::CheckRepairRelation(p, th, pf.z_star_open, z, meta)) return false;
	
		return true;
	}

} 
