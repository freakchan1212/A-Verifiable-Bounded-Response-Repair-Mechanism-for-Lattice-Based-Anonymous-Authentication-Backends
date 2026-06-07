
#include "zk/repair_boundary.h"

#include "common/bytes.h"
#include "common/hash.h"
#include "common/rng.h"
#include "lattice/mod_arith.h"

namespace zk {

	static inline int idx_of(const common::Params& p, int vec_i, int coeff_j) {
		return vec_i * p.N + coeff_j;
	}
	
	static std::vector<uint8_t> serialize_polyvec_i16_centered(const common::Params& p, const lattice::PolyVec& v) {
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
		out.reserve(meta.high.size() * 2 + meta.low.size() * 2 + 12);
	
		common::bytes::append_u32(out, (uint32_t)meta.high.size());
		for (auto v : meta.high) common::bytes::append_i16(out, v);
	
		common::bytes::append_u32(out, (uint32_t)meta.low.size());
		for (auto v : meta.low) common::bytes::append_i16(out, v);
	
		common::bytes::append_u32(out, (uint32_t)meta.clamp_count);
		return out;
	}
	
	uint64_t DigestBoundaryProof(const BoundaryProof& pf) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "BoundaryPf-v1");
	
		common::bytes::append_u32(buf, (uint32_t)pf.b_idx.size());
		for (size_t i = 0; i < pf.b_idx.size(); i++) {
			common::bytes::append_u32(buf, pf.b_idx[i]);
			common::bytes::append_i16(buf, pf.b_val[i]);
		}
	
		common::bytes::append_u32(buf, (uint32_t)pf.a_idx.size());
		for (size_t i = 0; i < pf.a_idx.size(); i++) {
			common::bytes::append_u32(buf, pf.a_idx[i]);
			common::bytes::append_i16(buf, pf.a_val[i]);
		}
	
		return common::hash::hash64(buf);
	}
	
	
	static uint64_t audit_seed(const common::Params& p,
		uint32_t nonce,
		const lattice::PolyVec& C,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "AuditSeed-v1");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_u32(buf, nonce);
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, C));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, z));
		common::bytes::append_bytes(buf, serialize_meta_i16(meta));
		return common::hash::hash64(buf);
	}
	
	static std::vector<uint32_t> pick_audit_indices(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z,
		int t,
		common::Rng& rng) {
		std::vector<uint32_t> interior;
		interior.reserve((size_t)p.k * (size_t)p.N);
	
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = z.v[i].c[j]; 
				if (x > -th.B && x < th.B) interior.push_back((uint32_t)idx_of(p, i, j));
			}
		}
	
		std::vector<uint32_t> out;
		if (interior.empty() || t <= 0) return out;
	
		while ((int)out.size() < t && (int)out.size() < (int)interior.size()) {
			uint32_t pick = (uint32_t)rng.uniform_int(0, (int)interior.size() - 1);
			uint32_t idx = interior[pick];
			bool seen = false;
			for (auto u : out) if (u == idx) { seen = true; break; }
			if (!seen) out.push_back(idx);
		}
		return out;
	}
	
	static int get_centered_at(const common::Params& p, const lattice::PolyVec& v, uint32_t flat_idx) {
		int i = (int)(flat_idx / (uint32_t)p.N);
		int j = (int)(flat_idx % (uint32_t)p.N);
		return lattice::center_q(v.v[i].c[j], p.q);
	}
	
	BoundaryProof ProveBoundaryToy(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_star,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta,
		uint32_t nonce,
		const lattice::PolyVec& C,
		int audit_t) {
		BoundaryProof pf;
	
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = z.v[i].c[j];
				if (x == th.B || x == -th.B) {
					uint32_t idx = (uint32_t)idx_of(p, i, j);
					pf.b_idx.push_back(idx);
					pf.b_val.push_back((int16_t)get_centered_at(p, z_star, idx));
				}
			}
		}
	
		uint64_t sd = audit_seed(p, nonce, C, z, meta);
		common::Rng arng(sd);
		pf.a_idx = pick_audit_indices(p, th, z, audit_t, arng);
		pf.a_val.reserve(pf.a_idx.size());
		for (uint32_t idx : pf.a_idx) {
			pf.a_val.push_back((int16_t)get_centered_at(p, z_star, idx));
		}
	
		return pf;
	}
	
	bool VerifyBoundaryToy(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z,
		const preauth::RepairMeta& meta,
		uint32_t nonce,
		const lattice::PolyVec& C,
		const BoundaryProof& pf,
		int audit_t) {
		std::vector<uint32_t> expected_b;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = z.v[i].c[j];
				if (x == th.B || x == -th.B) expected_b.push_back((uint32_t)idx_of(p, i, j));
			}
		}
	
		if (pf.b_idx.size() != expected_b.size()) return false;
		if (pf.b_val.size() != expected_b.size()) return false;
		for (size_t i = 0; i < expected_b.size(); i++) {
			if (pf.b_idx[i] != expected_b[i]) return false;
		}
	
		for (size_t t = 0; t < pf.b_idx.size(); t++) {
			uint32_t idx = pf.b_idx[t];
			int zcoef = z.v[idx / (uint32_t)p.N].c[idx % (uint32_t)p.N];
			int zstar = (int)pf.b_val[t];
	
			if (zcoef == th.B) {
				if (zstar < th.B) return false;
			}
			else if (zcoef == -th.B) {
				if (zstar > -th.B) return false;
			}
			else {
				return false;
			}
		}
	
		uint64_t sd = audit_seed(p, nonce, C, z, meta);
		common::Rng arng(sd);
		std::vector<uint32_t> expected_a = pick_audit_indices(p, th, z, audit_t, arng);
	
		if (pf.a_idx.size() != expected_a.size()) return false;
		if (pf.a_val.size() != expected_a.size()) return false;
		for (size_t i = 0; i < expected_a.size(); i++) {
			if (pf.a_idx[i] != expected_a[i]) return false;
		}
	
		for (size_t t = 0; t < pf.a_idx.size(); t++) {
			uint32_t idx = pf.a_idx[t];
			int zcoef = z.v[idx / (uint32_t)p.N].c[idx % (uint32_t)p.N];
			int zstar = (int)pf.a_val[t];
			if (zstar != zcoef) return false;
		}
	
		return true;
	}

} 
