#include "repair.h"

#include "common/bytes.h"
#include "common/hash.h"
#include "lattice/mod_arith.h"

#include <cstdlib>

namespace preauth {

	static inline int abs_i(int x) { return x < 0 ? -x : x; }
	
	
	
	static inline void split_hi_lo(int x, int d, int16_t& hi, int16_t& lo) {
		if (d <= 0) {
			hi = (int16_t)x;
			lo = 0;
			return;
		}
		
		int add = (1 << (d - 1));
		int hi_i = (x + add) >> d;
		int lo_i = x - (hi_i << d);
		
		hi = (int16_t)hi_i;
		lo = (int16_t)lo_i;
	}
	
	int polyvec_l0_centered(const common::Params& p, const lattice::PolyVec& v) {
		int l0 = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = lattice::center_q((int)v.v[(size_t)i].c[(size_t)j], p.q);
				if (x != 0) l0++;
			}
		}
		return l0;
	}
	
	int polyvec_linf_centered(const common::Params& p, const lattice::PolyVec& v) {
		int linf = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = lattice::center_q((int)v.v[(size_t)i].c[(size_t)j], p.q);
				int ax = abs_i(x);
				if (ax > linf) linf = ax;
			}
		}
		return linf;
	}
	
	static std::vector<uint8_t> serialize_polyvec_i16_centered(const common::Params& p,
		const lattice::PolyVec& v) {
		std::vector<uint8_t> out;
		out.reserve((size_t)p.k * (size_t)p.N * 2);
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = lattice::center_q((int)v.v[(size_t)i].c[(size_t)j], p.q);
				common::bytes::append_i16(out, (int16_t)x);
			}
		}
		return out;
	}
	
	std::vector<uint8_t> digest_polyvec_centered(const common::Params& p, const lattice::PolyVec& v) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "RepairDeltaDigest-v1");
		common::bytes::append_u32(buf, (uint32_t)p.N);
		common::bytes::append_u32(buf, (uint32_t)p.q);
		common::bytes::append_u32(buf, (uint32_t)p.k);
		std::vector<uint8_t> body = serialize_polyvec_i16_centered(p, v);
		common::bytes::append_bytes(buf, body);
		return common::hash::hash32(buf);
	}
	
	static std::vector<uint8_t> repair_digest_core(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z,
		const lattice::PolyVec& delta,
		const RepairMeta& meta) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "RepairAugmentedMetaDigest-v1");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_u32(buf, (uint32_t)p.N);
		common::bytes::append_u32(buf, (uint32_t)p.q);
		common::bytes::append_u32(buf, (uint32_t)p.k);
		common::bytes::append_u32(buf, (uint32_t)th.B);
		common::bytes::append_u32(buf, (uint32_t)th.d);
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, z));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, delta));
		common::bytes::append_u32(buf, (uint32_t)meta.high.size());
		for (size_t i = 0; i < meta.high.size(); i++) common::bytes::append_i16(buf, meta.high[i]);
		common::bytes::append_u32(buf, (uint32_t)meta.low.size());
		for (size_t i = 0; i < meta.low.size(); i++) common::bytes::append_i16(buf, meta.low[i]);
		common::bytes::append_u32(buf, (uint32_t)meta.clamp_count);
		common::bytes::append_u32(buf, (uint32_t)meta.delta_l0);
		common::bytes::append_u32(buf, (uint32_t)meta.delta_linf);
		common::bytes::append_bytes(buf, meta.delta_digest);
		common::bytes::append_bytes(buf, meta.ctx_digest);
		common::bytes::append_u64(buf, meta.refresh_tag);
		return common::hash::hash32(buf);
	}
	
	RepairResult repair_polyvec(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z_star) {
		RepairResult rr;
		rr.z = lattice::vec_zero(p);
		rr.delta = lattice::vec_zero(p);
		
		rr.meta.high.clear();
		rr.meta.low.clear();
		rr.meta.clamp_count = 0;
		rr.meta.delta_l0 = 0;
		rr.meta.delta_linf = 0;
		rr.meta.delta_digest.clear();
		rr.meta.ctx_digest.clear();
		rr.meta.refresh_tag = 0;
		rr.meta.repair_digest.clear();
		rr.meta.high.reserve((size_t)p.k * (size_t)p.N);
		rr.meta.low.reserve((size_t)p.k * (size_t)p.N);
		
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				const int raw = lattice::center_q((int)z_star.v[(size_t)i].c[(size_t)j], p.q);
				int zc = raw;
				if (raw > th.B) { zc = th.B; rr.meta.clamp_count++; }
				else if (raw < -th.B) { zc = -th.B; rr.meta.clamp_count++; }
				
				const int dc = raw - zc; 
				rr.z.v[(size_t)i].c[(size_t)j] = (lattice::Poly::coeff_t)zc;
				rr.delta.v[(size_t)i].c[(size_t)j] = (lattice::Poly::coeff_t)dc;
				
				if (dc != 0) rr.meta.delta_l0++;
				if (abs_i(dc) > rr.meta.delta_linf) rr.meta.delta_linf = abs_i(dc);
				
				int16_t hi = 0, lo = 0;
				split_hi_lo(zc, th.d, hi, lo);
				rr.meta.high.push_back(hi);
				rr.meta.low.push_back(lo);
			}
		}
		
		rr.repaired = (rr.meta.clamp_count > 0);
		rr.delta_l0 = rr.meta.delta_l0;
		rr.delta_linf = rr.meta.delta_linf;
		rr.delta_digest = digest_polyvec_centered(p, rr.delta);
		rr.meta.delta_digest = rr.delta_digest;
		return rr;
	}
	
	bool check_repair(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z,
		const RepairMeta& meta) {
		const int need = p.k * p.N;
		if ((int)meta.high.size() != need) return false;
		if ((int)meta.low.size() != need) return false;
		
		int idx = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++, idx++) {
				int x = lattice::center_q((int)z.v[(size_t)i].c[(size_t)j], p.q);
				if (x > th.B || x < -th.B) return false;
				
				int16_t hi = 0, lo = 0;
				split_hi_lo(x, th.d, hi, lo);
				if (meta.high[(size_t)idx] != hi) return false;
				if (meta.low[(size_t)idx] != lo) return false;
			}
		}
		return true;
	}
	
	bool check_repair_delta(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z,
		const lattice::PolyVec& delta,
		const RepairMeta& meta) {
		if (!check_repair(p, th, z, meta)) return false;
		
		int clamp_count = 0;
		int delta_l0 = 0;
		int delta_linf = 0;
		
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				const int zc = lattice::center_q((int)z.v[(size_t)i].c[(size_t)j], p.q);
				const int dc = lattice::center_q((int)delta.v[(size_t)i].c[(size_t)j], p.q);
				const int raw = zc + dc;
				
				int expected_z = raw;
				if (raw > th.B) { expected_z = th.B; clamp_count++; }
				else if (raw < -th.B) { expected_z = -th.B; clamp_count++; }
				
				const int expected_delta = raw - expected_z;
				if (zc != expected_z) return false;
				if (dc != expected_delta) return false;
				
				if (dc != 0) delta_l0++;
				if (abs_i(dc) > delta_linf) delta_linf = abs_i(dc);
			}
		}
		
		if (meta.clamp_count != clamp_count) return false;
		if (meta.delta_l0 != delta_l0) return false;
		if (meta.delta_linf != delta_linf) return false;
		if (meta.delta_digest != digest_polyvec_centered(p, delta)) return false;
		
		
		if (meta.ctx_digest.size() != 32) return false;
		if (meta.repair_digest.size() != 32) return false;
		if (meta.repair_digest != repair_digest_core(p, th, z, delta, meta)) return false;
		return true;
	}
	
	bool check_repair_context_binding(const RepairMeta& meta,
		const std::vector<uint8_t>& expected_ctx_digest,
		uint64_t expected_refresh_tag) {
		if (expected_ctx_digest.size() != 32) return false;
		if (meta.ctx_digest != expected_ctx_digest) return false;
		if (meta.refresh_tag != expected_refresh_tag) return false;
		return true;
	}
	
	void bind_repair_meta(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z,
		const lattice::PolyVec& delta,
		const std::vector<uint8_t>& ctx_digest,
		uint64_t refresh_tag,
		RepairMeta& meta) {
		meta.delta_l0 = polyvec_l0_centered(p, delta);
		meta.delta_linf = polyvec_linf_centered(p, delta);
		meta.delta_digest = digest_polyvec_centered(p, delta);
		meta.ctx_digest = ctx_digest;
		meta.refresh_tag = refresh_tag;
		meta.repair_digest = repair_digest_core(p, th, z, delta, meta);
	}
	
	lattice::PolyVec recover_zstar_from_repair(const common::Params& p,
		const lattice::PolyVec& z,
		const lattice::PolyVec& delta) {
		lattice::PolyVec out = lattice::vec_zero(p);
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int zc = lattice::center_q((int)z.v[(size_t)i].c[(size_t)j], p.q);
				int dc = lattice::center_q((int)delta.v[(size_t)i].c[(size_t)j], p.q);
				out.v[(size_t)i].c[(size_t)j] = (lattice::Poly::coeff_t)lattice::modq(zc + dc, p.q);
			}
		}
		return out;
	}
	
	size_t repair_meta_payload_bytes(const RepairMeta& meta) {
		return meta.high.size() * sizeof(int16_t)
			+ meta.low.size() * sizeof(int16_t)
			+ sizeof(int32_t)  
			+ sizeof(int32_t)  
			+ sizeof(int32_t)  
			+ meta.delta_digest.size()
			+ meta.ctx_digest.size()
			+ sizeof(uint64_t)
			+ meta.repair_digest.size();
	}

} 
