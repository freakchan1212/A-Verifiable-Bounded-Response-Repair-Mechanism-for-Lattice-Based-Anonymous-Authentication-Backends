
#include "zk/repair_sigma.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "common/bytes.h"
#include "common/hash.h"
#include "common/log.h"
#include "lattice/mod_arith.h"
#include "zk/lattice_commit.h"
#include "zk/proof_engine.h"

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
	
	static int total_coeffs(const common::Params& p) {
		return p.k * p.N;
	}
	
	static lattice::PolyVec to_modq(const common::Params& p,
		const lattice::PolyVec& v_centered) {
		lattice::PolyVec out = v_centered;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				out.v[i].c[j] = lattice::modq(out.v[i].c[j], p.q);
			}
		}
		return out;
	}
	
	static lattice::Poly poly_scalar_mul_modq(const common::Params& p,
		const lattice::Poly& a,
		uint32_t k) {
		lattice::Poly r(p.N);
		for (int i = 0; i < p.N; i++) {
			long long prod = (long long)k * (long long)a.c[i];
			r.c[i] = lattice::modq((int)prod, p.q);
		}
		return r;
	}
	
	static lattice::PolyVec polyvec_scalar_mul_modq(const common::Params& p,
		const lattice::PolyVec& v,
		uint32_t k) {
		lattice::PolyVec out = lattice::vec_zero(p);
		for (int i = 0; i < p.k; i++) out.v[i] = poly_scalar_mul_modq(p, v.v[i], k);
		return out;
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
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_centered,
		std::vector<uint8_t>& D_boundary,
		std::vector<uint8_t>& M_interior,
		std::vector<int>& off_centered,
		std::vector<int>& sign) {
		const int T = total_coeffs(p);
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
	
	static int max_boundary_overflow(const common::Params& p,
		const preauth::RepairTheta& th,
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

	static uint32_t count_boundary_positions(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_centered) {
		uint32_t cnt = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int zc = z_centered.v[i].c[j];
				if (zc == th.B || zc == -th.B) cnt++;
			}
		}
		return cnt;
	}

	static uint32_t count_interior_positions(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_centered) {
		uint32_t cnt = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int zc = z_centered.v[i].c[j];
				if (zc > -th.B && zc < th.B) cnt++;
			}
		}
		return cnt;
	}
	
	static uint32_t min_bits_for_nonnegative(int x) {
		if (x <= 0) return 1;
		uint32_t bits = 0;
		while (x > 0) {
			bits++;
			x >>= 1;
		}
		return bits;
	}
	
	static int pow2_modq(const common::Params& p, uint32_t b) {
		int acc = 1;
		for (uint32_t i = 0; i < b; i++) {
			acc = lattice::modq(acc + acc, p.q);
		}
		return acc;
	}
	
	static std::vector<std::vector<int> > build_eq3_diags(const common::Params& p,
		const std::vector<uint8_t>& D,
		const std::vector<int>& sign,
		uint32_t Lbits) {
		std::vector<std::vector<int> > diags;
		diags.reserve(Lbits);
		for (uint32_t b = 0; b < Lbits; b++) {
			const int two = pow2_modq(p, b);
			std::vector<int> diag(total_coeffs(p), 0);
			for (int t = 0; t < total_coeffs(p); t++) {
				if (D[t]) diag[t] = lattice::modq(-sign[t] * two, p.q);
			}
			diags.push_back(diag);
		}
		return diags;
	}
	
	static std::vector<lattice::PolyVec> build_bit_planes(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_centered,
		const lattice::PolyVec& zstar_centered,
		uint32_t Lbits) {
		std::vector<uint8_t> D, M;
		std::vector<int> off, sign;
		build_masks(p, th, z_centered, D, M, off, sign);
	
		if (Lbits == 0) Lbits = 1;
	
		std::vector<lattice::PolyVec> bits;
		bits.reserve(Lbits);
		for (uint32_t b = 0; b < Lbits; b++) bits.push_back(lattice::vec_zero(p));
	
		int idx = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++, idx++) {
				if (!D[idx]) continue;
	
				int x = lattice::center_q(zstar_centered.v[i].c[j], p.q);
				int u = 0;
	
				if (z_centered.v[i].c[j] == th.B) {
					u = x - th.B;
					if (u < 0) u = 0;
				}
				else {
					u = (-th.B) - x;
					if (u < 0) u = 0;
				}
	
				for (uint32_t b = 0; b < Lbits; b++) {
					bits[b].v[i].c[j] = ((u >> b) & 1);
				}
			}
		}
		return bits;
	}
	
	static bool validate_bit_planes_exact(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_centered,
		const lattice::PolyVec& zstar_centered,
		const std::vector<lattice::PolyVec>& bits,
		uint32_t Lbits) {
		if (bits.size() != Lbits) return false;
	
		for (uint32_t b = 0; b < Lbits; b++) {
			if ((int)bits[b].v.size() != p.k) return false;
			for (int i = 0; i < p.k; i++) {
				if ((int)bits[b].v[i].c.size() != p.N) return false;
			}
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
	
		

	
	static lattice::PolyVec derive_y1_public(const common::Params& p,
		const lattice::PublicKey& pk,
		const lattice::PolyVec& zstar_modq) {
		return lattice::mat_vec_mul(p, pk.A, zstar_modq);
	}
	
	static lattice::PolyVec derive_y3_public(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_centered) {
		std::vector<uint8_t> D, M;
		std::vector<int> off, sign;
		build_masks(p, th, z_centered, D, M, off, sign);
	
		lattice::PolyVec y3 = lattice::vec_zero(p);
		int idx = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++, idx++) {
				if (D[idx]) y3.v[i].c[j] = lattice::modq(off[idx], p.q);
			}
		}
		return y3;
	}
	
	static lattice::PolyVec derive_y4_public(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_centered) {
		std::vector<uint8_t> D, M;
		std::vector<int> off, sign;
		build_masks(p, th, z_centered, D, M, off, sign);
	
		lattice::PolyVec z_modq = to_modq(p, z_centered);
		return apply_mask01_modq(p, z_modq, M);
	}
	
	static int derive_challenge_scalar(const common::Params& p,
		const preauth::CommitMsg& cm,
		const lattice::PolyVec& C,
		const lattice::PolyVec& r_small,
		const lattice::PolyVec& y1,
		const lattice::PolyVec& y3,
		const lattice::PolyVec& y4,
		const RepairSigmaProof& pf) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "RepairSigma-v6");
		common::bytes::append_u64(buf, p.seed);
		common::bytes::append_u32(buf, cm.nonce);
		common::bytes::append_u64(buf, cm.scope_tag);
		common::bytes::append_u64(buf, cm.pseudonym);
		common::bytes::append_u64(buf, cm.refresh_tag);
	
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, cm.w));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, C));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, r_small));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, y1));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, y3));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, y4));
	
		common::bytes::append_u32(buf, pf.L);
		common::bytes::append_u32(buf, pf.backend_kind);
		common::bytes::append_u32(buf, pf.relation_kind);
		common::bytes::append_u32(buf, pf.exact_mode);
		common::bytes::append_u32(buf, pf.exact_u_max);
		common::bytes::append_u32(buf, pf.exact_boundary_count);
		common::bytes::append_u32(buf, pf.exact_interior_count);
		common::bytes::append_bytes(buf, pf.relation_digest);
		common::bytes::append_bytes(buf, pf.exact_digest);
		common::bytes::append_bytes(buf, pf.exact_proof_blob);
	
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, pf.a1));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, pf.a2));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, pf.a3));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, pf.a4));
	
		uint64_t h = common::hash::hash64(buf);
		if (p.q <= 2) return (int)(h % (uint64_t)p.q);
		return (int)(h % (uint64_t)(p.q - 1)) + 1;
	}
	
	RepairSigmaProof ProveRepairSigma(const common::Params& p,
		const lattice::PublicKey& pk,
		const preauth::CommitMsg& cm,
		const lattice::PolyVec& C,
		const lattice::PolyVec& r_small,
		const lattice::PolyVec& z_public_centered,
		const preauth::RepairMeta& ,
		const lattice::PolyVec& zstar_modq,
		const lattice::PolyVec& r_wit_modq,
		const lattice::PolyVec& zstar_centered,
		const preauth::RepairTheta& th,
		uint32_t Lbits,
		int ,
		common::Rng& rng) {
		RepairSigmaProof pf;
	
		const int max_u = max_boundary_overflow(p, th, z_public_centered, zstar_centered);
		const uint32_t need_L = min_bits_for_nonnegative(max_u);
		if (Lbits < need_L) Lbits = need_L;
		if (Lbits == 0) Lbits = 1;
		pf.L = Lbits;
	
		lattice::PolyMat Acom = DeriveCommitKey(p, pk);
	
		lattice::PolyVec y1 = derive_y1_public(p, pk, zstar_modq);
		lattice::PolyVec y3 = derive_y3_public(p, th, z_public_centered);
		lattice::PolyVec y4 = derive_y4_public(p, th, z_public_centered);
	
		lattice::PolyVec z_wit = lattice::vec_mod_reduce(p, zstar_modq);
		lattice::PolyVec r_wit = lattice::vec_mod_reduce(p, r_wit_modq);
		std::vector<lattice::PolyVec> bit_wit = build_bit_planes(p, th, z_public_centered, zstar_centered, Lbits);
	
		if (!validate_bit_planes_exact(p, th, z_public_centered, zstar_centered, bit_wit, Lbits)) {
			LOGE("[RepairSigma] exact boundary decomposition self-check failed");
			return RepairSigmaProof{};
		}
	
		pf.backend_kind = (p.zk_backend <= 1 ? ZK_BACKEND_LEGACY_EXACT_BLOB : ZK_BACKEND_RELATION_V1);
		pf.relation_kind = REPAIR_RELATION_FULL_V1;
		pf.exact_mode = (pf.backend_kind == ZK_BACKEND_LEGACY_EXACT_BLOB
			? EXACT_DIGEST_BINDING
			: EXACT_BACKEND_INTERFACE);
		pf.exact_u_max = (uint32_t)max_u;
		pf.exact_boundary_count = count_boundary_positions(p, th, z_public_centered);
		pf.exact_interior_count = count_interior_positions(p, th, z_public_centered);

		RepairRelationIR ir = BuildRepairRelationIR(
			p, cm, C, r_small, y1, y3, y4, z_public_centered,
			zstar_centered, r_wit_modq, bit_wit, Lbits,
			pf.exact_u_max, pf.exact_boundary_count, pf.exact_interior_count);

		std::vector<uint8_t> relation_digest;
		std::vector<uint8_t> exact_digest;
		std::vector<uint8_t> proof_blob;
		if (!BuildExactBackendArtifacts(p, pk, th, ir, (uint32_t)ZK_BACKEND_RELATION_V1,
			&relation_digest, &exact_digest, &proof_blob)) {
			LOGE("[RepairSigma] exact backend artifact construction failed");
			return RepairSigmaProof{};
		}

		pf.relation_digest = relation_digest;
		pf.exact_digest = exact_digest;
		if (pf.backend_kind == ZK_BACKEND_RELATION_V1) {
			pf.exact_proof_blob = proof_blob;
		} else {
			pf.exact_proof_blob.clear();
		}

		LOGI("[RepairSigma] backend=%u exact_mode=%u relation_digest=%zu exact_digest=%zu exact_blob=%zu",
			(unsigned)pf.backend_kind,
			(unsigned)pf.exact_mode,
			pf.relation_digest.size(),
			pf.exact_digest.size(),
			pf.exact_proof_blob.size());
	
		lattice::PolyVec u_z = lattice::vec_uniform(p, rng);
		lattice::PolyVec u_r = lattice::vec_uniform(p, rng);
	
		std::vector<lattice::PolyVec> u_bits;
		u_bits.reserve(Lbits);
		for (uint32_t b = 0; b < Lbits; b++) u_bits.push_back(lattice::vec_uniform(p, rng));
	
		pf.a1 = lattice::mat_vec_mul(p, pk.A, u_z);
	
		pf.a2 = lattice::mat_vec_mul(p, Acom, u_r);
		pf.a2 = lattice::vec_add(p, pf.a2, u_z);
	
		std::vector<uint8_t> D, M;
		std::vector<int> off, sign;
		build_masks(p, th, z_public_centered, D, M, off, sign);
		std::vector<std::vector<int> > eq3_diags = build_eq3_diags(p, D, sign, Lbits);
	
		pf.a3 = apply_mask01_modq(p, u_z, D);
		for (uint32_t b = 0; b < Lbits; b++) {
			lattice::PolyVec term = apply_diag_modq(p, u_bits[b], eq3_diags[b]);
			pf.a3 = lattice::vec_add(p, pf.a3, term);
		}
	
		pf.a4 = apply_mask01_modq(p, u_z, M);
	
		int ch = derive_challenge_scalar(p, cm, C, r_small, y1, y3, y4, pf);
		if (ch == 0) ch = 1;
	
		pf.s_z = lattice::vec_add(p, u_z, polyvec_scalar_mul_modq(p, z_wit, ch));
		pf.s_r = lattice::vec_add(p, u_r, polyvec_scalar_mul_modq(p, r_wit, ch));
	
		pf.s_bits.clear();
		pf.s_bits.reserve(Lbits);
		for (uint32_t b = 0; b < Lbits; b++) {
			lattice::PolyVec sb = lattice::vec_add(p, u_bits[b], polyvec_scalar_mul_modq(p, bit_wit[b], ch));
			pf.s_bits.push_back(sb);
		}
	
		return pf;
	}

} 
