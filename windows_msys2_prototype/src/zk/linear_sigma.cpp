
#include "zk/linear_sigma.h"
#include "common/bytes.h"
#include "common/hash.h"
#include "lattice/mod_arith.h"
#include "lattice/poly.h"

namespace zk {

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
	
	
	static uint32_t derive_challenge(const common::Params& p,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const lattice::PolyVec& y,
		const lattice::PolyVec& a) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "LinSigma-v1");
		common::bytes::append_u32(buf, nonce);
		common::bytes::append_bytes(buf, bind_bytes);
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, y));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, a));
	
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
	
	LinearSigmaProof Prove(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyVec& y,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const lattice::PolyVec& z_witness_modq,
		common::Rng& rng) {
		
		lattice::PolyVec r = lattice::vec_uniform(p, rng);
	
		
		lattice::PolyVec a = lattice::mat_vec_mul(p, A, r);
	
		
		uint32_t ch = derive_challenge(p, nonce, bind_bytes, y, a);
	
		
		lattice::PolyVec chz = polyvec_scalar_mul_modq(p, z_witness_modq, ch);
		lattice::PolyVec s = lattice::vec_add(p, r, chz);
	
		LinearSigmaProof proof;
		proof.a = a;
		proof.s = s;
		return proof;
	}
	
	bool Verify(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyVec& y,
		uint32_t nonce,
		const std::vector<uint8_t>& bind_bytes,
		const LinearSigmaProof& proof) {
		uint32_t ch = derive_challenge(p, nonce, bind_bytes, y, proof.a);
	
		
		lattice::PolyVec left = lattice::mat_vec_mul(p, A, proof.s);
	
		
		lattice::PolyVec chy = polyvec_scalar_mul_modq(p, y, ch);
		lattice::PolyVec right = lattice::vec_add(p, proof.a, chy);
	
		return polyvec_equal_modq(p, left, right);
	}

} 
