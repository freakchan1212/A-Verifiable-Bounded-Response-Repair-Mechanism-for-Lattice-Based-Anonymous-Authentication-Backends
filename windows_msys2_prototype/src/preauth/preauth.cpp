#include "preauth.h"

#include "preauth/transcript.h"
#include "preauth/statement.h"

#include "common/bytes.h"
#include "common/hash.h"
#include "common/rng.h"
#include "common/util.h"
#include "common/log.h"

#include "lattice/mod_arith.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"

#include "zk/lattice_commit.h"
#include "zk/repair_sigma.h"

#include "offload/coded_offload.h"

#include <algorithm>

namespace preauth {

	static std::vector<uint8_t> serialize_polyvec_i16(const common::Params& p, const lattice::PolyVec& v) {
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
	
	static lattice::PolyVec to_modq(const common::Params& p, const lattice::PolyVec& v_centered) {
		lattice::PolyVec out = v_centered;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				out.v[i].c[j] = lattice::modq(out.v[i].c[j], p.q);
			}
		}
		return out;
	}
	
	static int maxabs_centered_polyvec(const common::Params& p, const lattice::PolyVec& v_modq_or_centered) {
		int m = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = lattice::center_q(v_modq_or_centered.v[i].c[j], p.q);
				int ax = (x < 0 ? -x : x);
				if (ax > m) m = ax;
			}
		}
		return m;
	}
	
	static lattice::PolyVec apply_mask01_modq(const common::Params& p,
		const lattice::PolyVec& v_modq,
		const std::vector<uint8_t>& mask01) {
		lattice::PolyVec out = lattice::vec_zero(p);
		int idx = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++, idx++) {
				int m = (idx < (int)mask01.size()) ? (mask01[idx] & 1) : 0;
				out.v[i].c[j] = lattice::modq(m * lattice::modq(v_modq.v[i].c[j], p.q), p.q);
			}
		}
		return out;
	}
	
	static lattice::PolyVec apply_diag_modq(const common::Params& p,
		const lattice::PolyVec& v_modq,
		const std::vector<int>& diag) {
		lattice::PolyVec out = lattice::vec_zero(p);
		int idx = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++, idx++) {
				int d = (idx < (int)diag.size()) ? diag[idx] : 0;
				long long prod = 1LL * lattice::modq(d, p.q) * lattice::modq(v_modq.v[i].c[j], p.q);
				out.v[i].c[j] = lattice::modq((int)prod, p.q);
			}
		}
		return out;
	}
	
	static void build_masks(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z_centered,
		std::vector<uint8_t>& D_boundary,
		std::vector<uint8_t>& M_interior,
		std::vector<int>& off_centered,
		std::vector<int>& sign) {
		const int T = p.k * p.N;
		D_boundary.assign(T, 0);
		M_interior.assign(T, 0);
		off_centered.assign(T, 0);
		sign.assign(T, 0);
	
		int idx = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++, idx++) {
				int x = z_centered.v[i].c[j];
				if (x == th.B) {
					D_boundary[idx] = 1;
					off_centered[idx] = th.B;
					sign[idx] = +1;
				}
				else if (x == -th.B) {
					D_boundary[idx] = 1;
					off_centered[idx] = -th.B;
					sign[idx] = -1;
				}
				else if (x > -th.B && x < th.B) {
					M_interior[idx] = 1;
				}
			}
		}
	}
	
	static void append_offload_ctx_bytes(std::vector<uint8_t>& buf, const common::Params& p);

	static uint64_t init_refresh_state(const common::Params& p, const lattice::PolyVec& t) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "InitRefreshState-v1");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, t));
		return common::hash::hash64(buf);
	}
	
	uint64_t DerivePseudonym(const common::Params& p, uint64_t scope_tag, const lattice::PolyVec& pk_t) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "ScopedPseudonym-v1");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_u64(buf, scope_tag);
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, pk_t));
		return common::hash::hash64(buf);
	}

	std::vector<uint8_t> BuildRepairContextDigest(const common::Params& p,
		const lattice::PolyVec& member_t,
		const CommitMsg& cm,
		const std::vector<int>& c_bits) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "RepairContextDigest-v1");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_u32(buf, (uint32_t)p.N);
		common::bytes::append_u32(buf, (uint32_t)p.q);
		common::bytes::append_u32(buf, (uint32_t)p.k);
		common::bytes::append_u64(buf, cm.scope_tag);
		common::bytes::append_u64(buf, cm.pseudonym);
		common::bytes::append_u32(buf, cm.nonce);
		common::bytes::append_u64(buf, cm.refresh_tag);
		append_offload_ctx_bytes(buf, p);
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, cm.w));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, member_t));
		common::bytes::append_u32(buf, (uint32_t)c_bits.size());
		for (size_t i = 0; i < c_bits.size(); i++) {
			common::bytes::append_u32(buf, (uint32_t)(c_bits[i] & 1));
		}
		return common::hash::hash32(buf);
	}
	
	static void append_offload_ctx_bytes(std::vector<uint8_t>& buf, const common::Params& p) {
		common::bytes::append_u32(buf, (uint32_t)p.offload_proto);
		common::bytes::append_u32(buf, (uint32_t)p.off_k);
		common::bytes::append_u32(buf, (uint32_t)p.off_r);
		common::bytes::append_u32(buf, (uint32_t)p.off_freivalds);
		common::bytes::append_u32(buf, (uint32_t)p.freivalds_rounds);
		common::bytes::append_u32(buf, (uint32_t)p.off_threaded);
		common::bytes::append_u32(buf, (uint32_t)p.off_workers);
		common::bytes::append_u32(buf, (uint32_t)p.off_delay_us);
		common::bytes::append_u32(buf, (uint32_t)p.off_delay_jitter_us);
		common::bytes::append_u32(buf, (uint32_t)p.off_delay_step_us);
		common::bytes::append_u32(buf, (uint32_t)p.off_straggler_pct);
		common::bytes::append_u32(buf, (uint32_t)p.off_malicious_pct);
	}
	
	static void transcript_append_offload_ctx(Transcript& tr, const common::Params& p) {
		tr.append_u32((uint32_t)p.offload_proto);
		tr.append_u32((uint32_t)p.off_k);
		tr.append_u32((uint32_t)p.off_r);
		tr.append_u32((uint32_t)p.off_freivalds);
		tr.append_u32((uint32_t)p.freivalds_rounds);
		tr.append_u32((uint32_t)p.off_threaded);
		tr.append_u32((uint32_t)p.off_workers);
		tr.append_u32((uint32_t)p.off_delay_us);
		tr.append_u32((uint32_t)p.off_delay_jitter_us);
		tr.append_u32((uint32_t)p.off_delay_step_us);
		tr.append_u32((uint32_t)p.off_straggler_pct);
		tr.append_u32((uint32_t)p.off_malicious_pct);
	}
	
	static uint64_t compute_refresh_tag(const common::Params& p,
		uint64_t refresh_state,
		uint32_t nonce,
		uint64_t scope_tag,
		uint64_t pseudonym,
		const lattice::PolyVec& w,
		const lattice::PolyVec& t) {
	
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "RefreshTag-v2");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_u64(buf, refresh_state);
		common::bytes::append_u32(buf, nonce);
		common::bytes::append_u64(buf, scope_tag);
		common::bytes::append_u64(buf, pseudonym);
	
		append_offload_ctx_bytes(buf, p);
	
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, w));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, t));
		return common::hash::hash64(buf);
	}
	
	static uint64_t epoch_refresh_state(const common::Params& p, uint64_t refresh_state, uint32_t epoch, size_t leak_accum) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "EpochRefresh-v2");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_u64(buf, refresh_state);
		common::bytes::append_u32(buf, epoch);
		common::bytes::append_u64(buf, (uint64_t)leak_accum);
		return common::hash::hash64(buf);
	}
	
	static size_t refresh_period_sessions(const common::Params& p) {
		int base = (p.zk_audit_t <= 0 ? 8 : p.zk_audit_t);
		if (base < 1) base = 1;
		return (size_t)base;
	}
	
	static int derive_repair_sigma_challenge(const common::Params& p,
		const CommitMsg& cm,
		const lattice::PolyVec& C,
		const lattice::PolyVec& r_small,
		const lattice::PolyVec& y,
		const lattice::PolyVec& y3,
		const lattice::PolyVec& y4,
		const zk::RepairSigmaProof& pf) {
	
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "RepairSigma-v6");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_u32(buf, cm.nonce);
		common::bytes::append_u64(buf, cm.refresh_tag);
	
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, cm.w));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, C));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, r_small));
	
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, y));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, y3));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, y4));
	
		common::bytes::append_u32(buf, pf.L);
		common::bytes::append_u32(buf, pf.exact_mode);
		common::bytes::append_u32(buf, pf.exact_u_max);
		common::bytes::append_bytes(buf, pf.exact_digest);
		common::bytes::append_bytes(buf, pf.exact_proof_blob);
	
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, pf.a1));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, pf.a2));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, pf.a3));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, pf.a4));
	
		uint64_t h = common::hash::hash64(buf);
		if (p.q <= 2) return (int)(h % (uint64_t)p.q);
		return (int)(h % (uint64_t)(p.q - 1)) + 1;
	}
	
	static std::vector<int> derive_challenge_bits_fs(const common::Params& p,
		const lattice::PolyVec& member_t,
		const CommitMsg& cm) {
	
		Transcript tr;
		tr.append_tag("PReAuth-v4");
		tr.append_u64(p.seed);
		tr.append_u32((uint32_t)p.N);
		tr.append_u32((uint32_t)p.q);
		tr.append_u32((uint32_t)p.k);
	
		tr.append_u64(cm.scope_tag);
		tr.append_u64(cm.pseudonym);
		transcript_append_offload_ctx(tr, p);
	
		tr.append_u32(cm.nonce);
		tr.append_u64(cm.refresh_tag);
		tr.append_bytes(serialize_polyvec_i16(p, cm.w));
		tr.append_bytes(serialize_polyvec_i16(p, member_t));
		return tr.challenge_bits(64);
	}
	
	static int pow2_modq_preauth(const common::Params& p, uint32_t b) {
		int acc = 1;
		for (uint32_t i = 0; i < b; i++) {
			acc = lattice::modq(acc + acc, p.q);
		}
		return acc;
	}
	
	static bool polyvec_shape_ok(const common::Params& p, const lattice::PolyVec& v) {
		if ((int)v.v.size() != p.k) return false;
		for (int i = 0; i < p.k; i++) {
			if ((int)v.v[i].c.size() != p.N) return false;
		}
		return true;
	}
	
	static bool polyvec_eq_modq(const common::Params& p, const lattice::PolyVec& a, const lattice::PolyVec& b) {
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				if (lattice::modq(a.v[i].c[j], p.q) != lattice::modq(b.v[i].c[j], p.q)) return false;
			}
		}
		return true;
	}
	
	static bool repair_sigma_shape_ok(const common::Params& p, const zk::RepairSigmaProof& pf) {
		if (pf.L == 0 || pf.L > 30) return false;
		if (pf.s_bits.size() != pf.L) return false;
	
		if (!(pf.exact_mode == zk::EXACT_NONE ||
			pf.exact_mode == zk::EXACT_DIGEST_BINDING ||
			pf.exact_mode == zk::EXACT_BACKEND_INTERFACE)) return false;
	
		if (pf.exact_mode == zk::EXACT_DIGEST_BINDING) {
			if (pf.exact_digest.size() != 16) return false;
			if (!pf.exact_proof_blob.empty()) return false;
		}
		if (pf.exact_mode == zk::EXACT_NONE) {
			if (!pf.exact_digest.empty()) return false;
			if (!pf.exact_proof_blob.empty()) return false;
		}
		if (pf.exact_mode == zk::EXACT_BACKEND_INTERFACE) {
			if (pf.exact_digest.size() != 16) return false;
			if (pf.exact_proof_blob.empty()) return false;
		}
	
		if (!polyvec_shape_ok(p, pf.a1)) return false;
		if (!polyvec_shape_ok(p, pf.a2)) return false;
		if (!polyvec_shape_ok(p, pf.a3)) return false;
		if (!polyvec_shape_ok(p, pf.a4)) return false;
		if (!polyvec_shape_ok(p, pf.s_z)) return false;
		if (!polyvec_shape_ok(p, pf.s_r)) return false;
	
		for (uint32_t b = 0; b < pf.L; b++) {
			if (!polyvec_shape_ok(p, pf.s_bits[b])) return false;
		}
		return true;
	}
	
	static std::vector<std::vector<int> > build_eq3_diags_preauth(const common::Params& p,
		const std::vector<uint8_t>& D,
		const std::vector<int>& sign,
		uint32_t Lbits) {
		std::vector<std::vector<int> > diags;
		diags.reserve(Lbits);
		for (uint32_t b = 0; b < Lbits; b++) {
			const int two = pow2_modq_preauth(p, b);
			std::vector<int> diag(p.k * p.N, 0);
			for (int t = 0; t < p.k * p.N; t++) {
				if (D[t]) diag[t] = lattice::modq(-sign[t] * two, p.q);
			}
			diags.push_back(diag);
		}
		return diags;
	}
	
	static int max_boundary_overflow_preauth(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z_centered,
		const lattice::PolyVec& zstar_centered) {
		int max_u = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int zc = z_centered.v[i].c[j];
				if (zc != th.B && zc != -th.B) continue;
				int xs = lattice::center_q(zstar_centered.v[i].c[j], p.q);
				int u = (zc == th.B) ? (xs - th.B) : ((-th.B) - xs);
				if (u < 0) u = 0;
				if (u > max_u) max_u = u;
			}
		}
		return max_u;
	}
	
	static bool validate_bit_planes_exact_preauth(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z_centered,
		const lattice::PolyVec& zstar_centered,
		const std::vector<lattice::PolyVec>& bits,
		uint32_t Lbits) {
		if (bits.size() != Lbits) return false;
	
		for (uint32_t b = 0; b < Lbits; b++) {
			if (!polyvec_shape_ok(p, bits[b])) return false;
		}
	
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				const int zc = z_centered.v[i].c[j];
				const int xs = lattice::center_q(zstar_centered.v[i].c[j], p.q);
	
				const bool is_boundary = (zc == th.B || zc == -th.B);
				const bool is_interior = (zc > -th.B && zc < th.B);
				if (!is_boundary && !is_interior) return false;
	
				int64_t recon_u = 0;
				for (uint32_t b = 0; b < Lbits; b++) {
					const int bit = lattice::center_q(bits[b].v[i].c[j], p.q);
					if (bit != 0 && bit != 1) return false;
					if (!is_boundary && bit != 0) return false;
					if (bit == 1) recon_u += (1LL << b);
				}
	
				if (is_interior) {
					if (xs != zc) return false;
					continue;
				}
	
				int expected_u = 0;
				if (zc == th.B) {
					if (xs < th.B) return false;
					expected_u = xs - th.B;
				}
				else {
					if (xs > -th.B) return false;
					expected_u = (-th.B) - xs;
				}
				if (expected_u < 0) return false;
				if (recon_u != (int64_t)expected_u) return false;
			}
		}
		return true;
	}
	
	static std::vector<uint8_t> derive_exact_digest_preauth(const common::Params& p,
		const lattice::PolyVec& z_public_centered,
		const lattice::PolyVec& zstar_centered,
		const lattice::PolyVec& r_wit_modq,
		const std::vector<lattice::PolyVec>& bit_wit,
		uint32_t Lbits,
		uint32_t exact_u_max) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "RepairSigmaExactDigest-v2");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_u32(buf, Lbits);
		common::bytes::append_u32(buf, exact_u_max);
	
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, z_public_centered));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, zstar_centered));
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, r_wit_modq));
	
		for (uint32_t b = 0; b < Lbits; b++) {
			common::bytes::append_bytes(buf, serialize_polyvec_i16(p, bit_wit[b]));
		}
	
		uint64_t h1 = common::hash::hash64(buf);
		common::bytes::append_u64(buf, h1);
		uint64_t h2 = common::hash::hash64(buf);
	
		std::vector<uint8_t> dg;
		dg.reserve(16);
		common::bytes::append_u64(dg, h1);
		common::bytes::append_u64(dg, h2);
		return dg;
	}
	
	static bool read_u32_blob(const std::vector<uint8_t>& blob, size_t& pos, uint32_t& x) {
		if (pos + 4 > blob.size()) return false;
		x = (uint32_t)blob[pos]
			| ((uint32_t)blob[pos + 1] << 8)
			| ((uint32_t)blob[pos + 2] << 16)
			| ((uint32_t)blob[pos + 3] << 24);
		pos += 4;
		return true;
	}
	
	static bool read_polyvec_i16_blob(const common::Params& p,
		const std::vector<uint8_t>& blob,
		size_t& pos,
		lattice::PolyVec& out) {
		out = lattice::vec_zero(p);
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				if (pos + 2 > blob.size()) return false;
				uint16_t lo = (uint16_t)blob[pos];
				uint16_t hi = (uint16_t)blob[pos + 1];
				int16_t x = (int16_t)(lo | (hi << 8));
				out.v[i].c[j] = (int)x;
				pos += 2;
			}
		}
		return true;
	}
	
	static bool exact_backend_interface_verify_toy(const common::Params& p,
		const lattice::PublicKey& pk_local,
		const RepairTheta& th,
		const lattice::PolyVec& z_public_centered,
		const lattice::PolyVec& zstar_recovered_modq,
		const lattice::PolyVec& C,
		const lattice::PolyVec& y,
		const zk::RepairSigmaProof& pf) {
	
		size_t pos = 0;
		uint32_t version = 0, blob_L = 0, blob_umax = 0;
		if (!read_u32_blob(pf.exact_proof_blob, pos, version)) return false;
		if (!read_u32_blob(pf.exact_proof_blob, pos, blob_L)) return false;
		if (!read_u32_blob(pf.exact_proof_blob, pos, blob_umax)) return false;
		if (version != 1u) return false;
		if (blob_L != pf.L) return false;
		if (blob_umax != pf.exact_u_max) return false;
	
		lattice::PolyVec zstar_centered;
		lattice::PolyVec r_wit_centered;
		if (!read_polyvec_i16_blob(p, pf.exact_proof_blob, pos, zstar_centered)) return false;
		if (!read_polyvec_i16_blob(p, pf.exact_proof_blob, pos, r_wit_centered)) return false;
	
		std::vector<lattice::PolyVec> bits;
		bits.resize(pf.L);
		for (uint32_t b = 0; b < pf.L; b++) {
			if (!read_polyvec_i16_blob(p, pf.exact_proof_blob, pos, bits[b])) return false;
		}
		if (pos != pf.exact_proof_blob.size()) return false;
	
		if (!validate_bit_planes_exact_preauth(p, th, z_public_centered, zstar_centered, bits, pf.L)) return false;
		lattice::PolyVec zstar_blob_modq = to_modq(p, zstar_centered);
		if (!polyvec_eq_modq(p, zstar_blob_modq, zstar_recovered_modq)) return false;
	
		int actual_umax = max_boundary_overflow_preauth(p, th, z_public_centered, zstar_centered);
		if ((uint32_t)actual_umax != pf.exact_u_max) return false;
	
		std::vector<uint8_t> dg = derive_exact_digest_preauth(
			p, z_public_centered, zstar_centered, r_wit_centered, bits, pf.L, pf.exact_u_max);
		if (dg != pf.exact_digest) return false;
	
		lattice::PolyVec zstar_modq = zstar_blob_modq;
		lattice::PolyVec y_blob = lattice::mat_vec_mul(p, pk_local.A, zstar_modq);
		if (!polyvec_eq_modq(p, y_blob, y)) return false;
	
		lattice::PolyMat Acom = zk::DeriveCommitKey(p, pk_local);
		lattice::PolyVec r_wit_modq = to_modq(p, r_wit_centered);
		lattice::PolyVec C_blob = zk::Commit(p, Acom, r_wit_modq, zstar_modq);
		if (!polyvec_eq_modq(p, C_blob, C)) return false;
	
		return true;
	}
	
	
	
	Prover::Prover(const common::Params& p, const lattice::PublicKey& pk, const lattice::SecretKey& sk)
		: p_(p), pk_(pk), sk_(sk) {
		y_ = lattice::vec_zero(p_);
		refresh_state_ = init_refresh_state(p_, pk_.t);
		epoch_ = 0;
		leak_accum_ = 0;
	
		scope_tag_ = 0;
		pseudonym_ = DerivePseudonym(p_, scope_tag_, pk_.t);
	}
	
	void Prover::set_scope(uint64_t scope_tag) {
		scope_tag_ = scope_tag;
		pseudonym_ = DerivePseudonym(p_, scope_tag_, pk_.t);
	}
	
	CommitMsg Prover::commit(common::Rng& rng) {
		y_ = lattice::vec_small(p_, rng);
	
		CommitMsg cm;
		cm.nonce = (uint32_t)rng.next_u32();
		cm.scope_tag = scope_tag_;
		cm.pseudonym = pseudonym_;
		cm.w = lattice::mat_vec_mul(p_, pk_.A, y_);
		cm.refresh_tag = compute_refresh_tag(p_, refresh_state_, cm.nonce, cm.scope_tag, cm.pseudonym, cm.w, pk_.t);
		last_commit_ = cm;
		return cm;
	}
	
	ResponseMsg Prover::respond(const std::vector<int>& c_bits) {
		return respond_impl(c_bits, nullptr);
	}
	
	ResponseMsg Prover::respond_impl(const std::vector<int>& c_bits, ProveObs* obs) {
		int c = 0;
		for (int b : c_bits) c ^= (b & 1);
	
		if (obs) obs->c_scalar = c;
	
		lattice::PolyVec z_star = y_;
		if (c == 1) {
			for (int i = 0; i < p_.k; i++) {
				for (int j = 0; j < p_.N; j++) {
					z_star.v[i].c[j] += sk_.s.v[i].c[j];
				}
			}
		}
	
		if (obs) obs->zstar_maxabs_centered = maxabs_centered_polyvec(p_, z_star);
	
		RepairTheta th{ p_.B, p_.d };
		RepairResult rr = repair_polyvec(p_, th, z_star);
		std::vector<uint8_t> ctx_digest = BuildRepairContextDigest(p_, pk_.t, last_commit_, c_bits);
		bind_repair_meta(p_, th, rr.z, rr.delta, ctx_digest, last_commit_.refresh_tag, rr.meta);
	
		if (obs) {
			obs->clamp_count = rr.meta.clamp_count;
			obs->delta_l0 = rr.meta.delta_l0;
			obs->delta_linf = rr.meta.delta_linf;
			obs->z_repaired_linf = polyvec_linf_centered(p_, rr.z);
		}
	
		lattice::PolyVec zstar_mod = to_modq(p_, z_star);
		lattice::PolyVec Azs = lattice::mat_vec_mul(p_, pk_.A, zstar_mod);
	
		lattice::PolyVec ct = lattice::vec_zero(p_);
		if (c == 1) ct = pk_.t;
	
		lattice::PolyVec tmp = lattice::vec_sub(p_, Azs, ct);
		lattice::PolyVec r_mod = lattice::vec_sub(p_, tmp, last_commit_.w);
		lattice::PolyVec r_small = lattice::vec_center_reduce(p_, r_mod);
	
		lattice::PolyMat Acom = zk::DeriveCommitKey(p_, pk_);
	
		std::vector<uint8_t> seedbuf;
		common::bytes::append_str(seedbuf, "ComRand-stage3");
		common::bytes::append_u64(seedbuf, p_.seed);
		common::bytes::append_u32(seedbuf, last_commit_.nonce);
		common::bytes::append_u64(seedbuf, last_commit_.refresh_tag);
		common::bytes::append_u64(seedbuf, last_commit_.scope_tag);
		common::bytes::append_u64(seedbuf, last_commit_.pseudonym);
		common::bytes::append_bytes(seedbuf, serialize_polyvec_i16(p_, rr.z));
		common::bytes::append_bytes(seedbuf, serialize_polyvec_i16(p_, rr.delta));
		common::bytes::append_bytes(seedbuf, rr.meta.repair_digest);
		uint64_t r_seed = common::hash::hash64(seedbuf);
		common::Rng r_rng(r_seed);
		lattice::PolyVec r_wit = lattice::vec_uniform(p_, r_rng);
	
		lattice::PolyVec C = zk::Commit(p_, Acom, r_wit, zstar_mod);
	
		uint32_t Lbits = (uint32_t)(p_.zk_Lbits <= 0 ? 8 : p_.zk_Lbits);
		int audit_t = (p_.zk_audit_t <= 0 ? 8 : p_.zk_audit_t);
	
		std::vector<uint8_t> prseed;
		common::bytes::append_str(prseed, "ProofRng-stage3");
		common::bytes::append_u64(prseed, p_.seed);
		common::bytes::append_u32(prseed, last_commit_.nonce);
		common::bytes::append_u64(prseed, last_commit_.refresh_tag);
		common::bytes::append_u64(prseed, last_commit_.scope_tag);
		common::bytes::append_u64(prseed, last_commit_.pseudonym);
		common::bytes::append_bytes(prseed, serialize_polyvec_i16(p_, C));
		common::bytes::append_bytes(prseed, serialize_polyvec_i16(p_, rr.delta));
		common::bytes::append_bytes(prseed, rr.meta.repair_digest);
		uint64_t proof_seed = common::hash::hash64(prseed);
		common::Rng proof_rng(proof_seed);
	
		zk::RepairSigmaProof proof = zk::ProveRepairSigma(
			p_, pk_, last_commit_, C, r_small, rr.z, rr.meta,
			zstar_mod, r_wit, z_star, th, Lbits, audit_t, proof_rng
		);
	
		if (proof.L == 0) {
			LOGE("[RepairSigma] prover received invalid empty proof after phase-4B-min");
		}
	
		if (obs) obs->audit_cnt = 0;
	
		ResponseMsg rsp;
		rsp.r_small = r_small;
		rsp.C = C;
		rsp.z = rr.z;
		rsp.delta = rr.delta;
		rsp.meta = rr.meta;
		rsp.proof = proof;
		return rsp;
	}
	
	void Prover::on_accept(const CommitMsg& cm, const ResponseMsg& ) {
		refresh_state_ = cm.refresh_tag;
	
		leak_accum_ += 1;
		size_t period = refresh_period_sessions(p_);
		if (period > 0 && leak_accum_ >= period) {
			epoch_ += 1;
			refresh_state_ = epoch_refresh_state(p_, refresh_state_, epoch_, leak_accum_);
			leak_accum_ = 0;
			LOGI("[Refresh] Prover epoch=%u", (unsigned)epoch_);
		}
	}
	
	AuthProof Prover::prove_nizk(common::Rng& rng) {
		AuthProof pf;
		pf.cm = commit(rng);
		std::vector<int> c_bits = derive_challenge_bits_fs(p_, pk_.t, pf.cm);
		pf.rsp = respond_impl(c_bits, nullptr);
		return pf;
	}
	
	AuthProof Prover::prove_nizk_observe(common::Rng& rng, ProveObs* obs) {
		if (obs) *obs = ProveObs();
		AuthProof pf;
		pf.cm = commit(rng);
		std::vector<int> c_bits = derive_challenge_bits_fs(p_, pk_.t, pf.cm);
		pf.rsp = respond_impl(c_bits, obs);
		return pf;
	}
	
	
	
	Verifier::Verifier(const common::Params& p, const lattice::PublicKey& pk)
		: p_(p), A_(pk.A), default_t_(pk.t) {
		set_scope(0);
	}
	
	void Verifier::set_scope(uint64_t scope_tag) {
		scope_tag_ = scope_tag;
		members_.clear();
		revoked_.clear();
		register_member(default_t_);
	}
	
	uint64_t Verifier::register_member(const lattice::PolyVec& t) {
		uint64_t nym = DerivePseudonym(p_, scope_tag_, t);
	
		MemberState st;
		st.t = t;
		st.refresh_state = init_refresh_state(p_, t);
		st.epoch = 0;
		st.leak_accum = 0;
	
		members_[nym] = st;
		return nym;
	}
	
	void Verifier::add_revoked(uint64_t pseudonym) {
		revoked_.insert(pseudonym);
	}
	
	bool Verifier::is_revoked(uint64_t pseudonym) const {
		return revoked_.find(pseudonym) != revoked_.end();
	}
	
	bool Verifier::has_member(uint64_t pseudonym) const {
		return members_.find(pseudonym) != members_.end();
	}
	
	uint32_t Verifier::member_epoch(uint64_t pseudonym) const {
		auto it = members_.find(pseudonym);
		if (it == members_.end()) return 0;
		return it->second.epoch;
	}
	
	size_t Verifier::member_leak_accum(uint64_t pseudonym) const {
		auto it = members_.find(pseudonym);
		if (it == members_.end()) return 0;
		return it->second.leak_accum;
	}
	
	uint64_t Verifier::member_refresh_state(uint64_t pseudonym) const {
		auto it = members_.find(pseudonym);
		if (it == members_.end()) return 0;
		return it->second.refresh_state;
	}
	
	std::vector<int> Verifier::derive_challenge(const CommitMsg& cm) {
		auto it = members_.find(cm.pseudonym);
		if (it == members_.end()) return {};
		return derive_challenge_bits_fs(p_, it->second.t, cm);
	}
	
	bool Verifier::verify_nizk(const AuthProof& pf) {
		std::vector<int> c_bits = derive_challenge(pf.cm);
		if (c_bits.empty()) return false;
		return verify(pf.cm, c_bits, pf.rsp);
	}
	
	bool Verifier::verify(const CommitMsg& cm, const std::vector<int>& c_bits, const ResponseMsg& rsp) {
		if (cm.scope_tag != scope_tag_) return false;
	
		auto it = members_.find(cm.pseudonym);
		if (it == members_.end()) return false;
	
		MemberState& ms = it->second;
	
		if (is_revoked(cm.pseudonym)) return false;
	
		uint64_t expected = compute_refresh_tag(p_, ms.refresh_state, cm.nonce, cm.scope_tag, cm.pseudonym, cm.w, ms.t);
		if (expected != cm.refresh_tag) return false;
	
		int c = 0;
		for (int b : c_bits) c ^= (b & 1);
	
		RepairTheta th{ p_.B, p_.d };
	
		if (!repair_sigma_shape_ok(p_, rsp.proof)) return false;
		std::vector<uint8_t> expected_ctx_digest = BuildRepairContextDigest(p_, ms.t, cm, c_bits);
		if (!check_repair_delta(p_, th, rsp.z, rsp.delta, rsp.meta)) return false;
		if (!check_repair_context_binding(rsp.meta, expected_ctx_digest, cm.refresh_tag)) return false;
		if (!preauth::stmt::CheckResidualSmallCentered(p_, rsp.r_small)) return false;
	
		lattice::PublicKey pk_local;
		pk_local.A = A_;
		pk_local.t = ms.t;
	
		lattice::PolyVec y = preauth::stmt::BuildYFromCommit(p_, pk_local, cm, c, rsp.r_small);
		lattice::PolyVec zstar_recovered_modq = recover_zstar_from_repair(p_, rsp.z, rsp.delta);
		lattice::PolyVec alg_y = lattice::mat_vec_mul(p_, pk_local.A, zstar_recovered_modq);
		if (!polyvec_eq_modq(p_, alg_y, y)) return false;
	
		if (rsp.proof.exact_mode == zk::EXACT_BACKEND_INTERFACE) {
			if (!exact_backend_interface_verify_toy(p_, pk_local, th, rsp.z, zstar_recovered_modq, rsp.C, y, rsp.proof)) return false;
		}
	
		std::vector<uint8_t> D, M;
		std::vector<int> off_centered, sign;
		build_masks(p_, th, rsp.z, D, M, off_centered, sign);
	
		lattice::PolyVec y3 = lattice::vec_zero(p_);
		int idx = 0;
		for (int i = 0; i < p_.k; i++) {
			for (int j = 0; j < p_.N; j++, idx++) {
				if (D[idx]) y3.v[i].c[j] = lattice::modq(off_centered[idx], p_.q);
			}
		}
	
		lattice::PolyVec z_modq = to_modq(p_, rsp.z);
		lattice::PolyVec y4 = apply_mask01_modq(p_, z_modq, M);
	
		int ch = derive_repair_sigma_challenge(p_, cm, rsp.C, rsp.r_small, y, y3, y4, rsp.proof);
		if (ch == 0) return false;
	
		auto scalar_mul = [&](const lattice::PolyVec& v, int k)->lattice::PolyVec {
			lattice::PolyVec out = lattice::vec_zero(p_);
			for (int i = 0; i < p_.k; i++) {
				for (int j = 0; j < p_.N; j++) {
					long long prod = 1LL * k * lattice::modq(v.v[i].c[j], p_.q);
					out.v[i].c[j] = lattice::modq((int)prod, p_.q);
				}
			}
			return out;
		};
	
		lattice::PolyMat Acom = zk::DeriveCommitKey(p_, pk_local);
	
		{
			lattice::PolyVec left = lattice::mat_vec_mul(p_, pk_local.A, rsp.proof.s_z);
			lattice::PolyVec right = lattice::vec_add(p_, rsp.proof.a1, scalar_mul(y, ch));
			if (!polyvec_eq_modq(p_, left, right)) return false;
		}
	
		{
			lattice::PolyVec left = lattice::mat_vec_mul(p_, Acom, rsp.proof.s_r);
			left = lattice::vec_add(p_, left, rsp.proof.s_z);
			lattice::PolyVec right = lattice::vec_add(p_, rsp.proof.a2, scalar_mul(rsp.C, ch));
			if (!polyvec_eq_modq(p_, left, right)) return false;
		}
	
		{
			lattice::PolyVec left = apply_mask01_modq(p_, rsp.proof.s_z, D);
			std::vector<std::vector<int> > eq3_diags = build_eq3_diags_preauth(p_, D, sign, rsp.proof.L);
			for (uint32_t b = 0; b < rsp.proof.L; b++) {
				lattice::PolyVec term = apply_diag_modq(p_, rsp.proof.s_bits[b], eq3_diags[b]);
				left = lattice::vec_add(p_, left, term);
			}
			lattice::PolyVec right = lattice::vec_add(p_, rsp.proof.a3, scalar_mul(y3, ch));
			if (!polyvec_eq_modq(p_, left, right)) return false;
		}
	
		{
			lattice::PolyVec left = apply_mask01_modq(p_, rsp.proof.s_z, M);
			lattice::PolyVec right = lattice::vec_add(p_, rsp.proof.a4, scalar_mul(y4, ch));
			if (!polyvec_eq_modq(p_, left, right)) return false;
		}
	
		ms.refresh_state = cm.refresh_tag;
	
		ms.leak_accum += 1;
		size_t period = refresh_period_sessions(p_);
		if (period > 0 && ms.leak_accum >= period) {
			ms.epoch += 1;
			ms.refresh_state = epoch_refresh_state(p_, ms.refresh_state, ms.epoch, ms.leak_accum);
			ms.leak_accum = 0;
			LOGI("[Refresh] Verifier epoch=%u", (unsigned)ms.epoch);
		}
	
		return true;
	}

} 
