
#include "zk/joint_sigma3.h"
#include "common/bytes.h"
#include "common/hash.h"
#include "lattice/mod_arith.h"
#include "lattice/poly.h"

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
	
	static uint32_t derive_challenge(const common::Params& p,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const lattice::PolyVec& y,
		const lattice::PolyVec& C,
		const lattice::PolyVec& y3,
		const JointSigma3Proof& pf) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "JointSigma3-v1");
		common::bytes::append_u32(buf, nonce);
		common::bytes::append_bytes(buf, bind_bytes);
	
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, y));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, C));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, y3));
	
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, pf.a1));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, pf.a2));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, pf.a3));
	
		uint64_t h = common::hash::hash64(buf);
		if (p.q <= 2) return (uint32_t)(h % (uint64_t)p.q);
		return (uint32_t)(h % (uint64_t)(p.q - 1)) + 1; 
	}
	
	static lattice::Poly poly_scalar_mul_modq(const common::Params& p, const lattice::Poly& a, uint32_t k) {
		lattice::Poly r(p.N);
		for (int i = 0; i < p.N; i++) {
			long long prod = (long long)k * (long long)a.c[i];
			r.c[i] = lattice::modq((int)prod, p.q);
		}
		return r;
	}
	
	static lattice::PolyVec polyvec_scalar_mul_modq(const common::Params& p, const lattice::PolyVec& v, uint32_t k) {
		lattice::PolyVec out = lattice::vec_zero(p);
		for (int i = 0; i < p.k; i++) out.v[i] = poly_scalar_mul_modq(p, v.v[i], k);
		return out;
	}
	
	static bool polyvec_equal_modq(const common::Params& p, const lattice::PolyVec& a, const lattice::PolyVec& b) {
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = lattice::modq(a.v[i].c[j], p.q);
				int y = lattice::modq(b.v[i].c[j], p.q);
				if (x != y) return false;
			}
		}
		return true;
	}
	
	
	static lattice::PolyVec apply_mask(const common::Params& p,
		const lattice::PolyVec& v,
		const std::vector<uint8_t>& mask01) {
		lattice::PolyVec out = lattice::vec_zero(p);
		int idx = 0;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++, idx++) {
				int m = (idx < (int)mask01.size()) ? (mask01[idx] & 1) : 0;
				out.v[i].c[j] = lattice::modq(m * lattice::modq(v.v[i].c[j], p.q), p.q);
			}
		}
		return out;
	}
	
	JointSigma3Proof ProveJoint3(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyMat& Acom,
		const std::vector<uint8_t>& mask01,
		const lattice::PolyVec& y,
		const lattice::PolyVec& C,
		const lattice::PolyVec& y3,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const lattice::PolyVec& z_wit_modq,
		const lattice::PolyVec& r_wit_modq,
		common::Rng& rng) {
		lattice::PolyVec u_z = lattice::vec_uniform(p, rng);
		lattice::PolyVec u_r = lattice::vec_uniform(p, rng);
	
		JointSigma3Proof pf;
		pf.a1 = lattice::mat_vec_mul(p, A, u_z);
	
		lattice::PolyVec Acur = lattice::mat_vec_mul(p, Acom, u_r);
		pf.a2 = lattice::vec_add(p, Acur, u_z);
	
		pf.a3 = apply_mask(p, u_z, mask01);
	
		uint32_t ch = derive_challenge(p, nonce, bind_bytes, y, C, y3, pf);
	
		pf.s_z = lattice::vec_add(p, u_z, polyvec_scalar_mul_modq(p, z_wit_modq, ch));
		pf.s_r = lattice::vec_add(p, u_r, polyvec_scalar_mul_modq(p, r_wit_modq, ch));
		return pf;
	}
	
	bool VerifyJoint3(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyMat& Acom,
		const std::vector<uint8_t>& mask01,
		const lattice::PolyVec& y,
		const lattice::PolyVec& C,
		const lattice::PolyVec& y3,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const JointSigma3Proof& pf) {
		uint32_t ch = derive_challenge(p, nonce, bind_bytes, y, C, y3, pf);
	
		
		lattice::PolyVec left1 = lattice::mat_vec_mul(p, A, pf.s_z);
		lattice::PolyVec right1 = lattice::vec_add(p, pf.a1, polyvec_scalar_mul_modq(p, y, ch));
		if (!polyvec_equal_modq(p, left1, right1)) return false;
	
		
		lattice::PolyVec left2 = lattice::mat_vec_mul(p, Acom, pf.s_r);
		left2 = lattice::vec_add(p, left2, pf.s_z);
		lattice::PolyVec right2 = lattice::vec_add(p, pf.a2, polyvec_scalar_mul_modq(p, C, ch));
		if (!polyvec_equal_modq(p, left2, right2)) return false;
	
		
		lattice::PolyVec left3 = apply_mask(p, pf.s_z, mask01);
		lattice::PolyVec right3 = lattice::vec_add(p, pf.a3, polyvec_scalar_mul_modq(p, y3, ch));
		if (!polyvec_equal_modq(p, left3, right3)) return false;
	
		return true;
	}

} 
