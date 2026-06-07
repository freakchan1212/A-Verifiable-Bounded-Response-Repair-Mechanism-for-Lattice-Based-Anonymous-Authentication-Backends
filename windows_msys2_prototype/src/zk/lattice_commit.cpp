
#include "zk/lattice_commit.h"
#include "common/bytes.h"
#include "common/hash.h"
#include "common/rng.h"
#include "lattice/mod_arith.h"

namespace zk {

	static std::vector<uint8_t> serialize_poly_i16_centered(const common::Params& p,
		const lattice::Poly& a) {
		std::vector<uint8_t> out;
		out.reserve((size_t)p.N * 2);
		for (int i = 0; i < p.N; i++) {
			int x = lattice::center_q(a.c[i], p.q);
			common::bytes::append_i16(out, (int16_t)x);
		}
		return out;
	}
	
	static std::vector<uint8_t> serialize_polyvec_i16_centered(const common::Params& p,
		const lattice::PolyVec& v) {
		std::vector<uint8_t> out;
		out.reserve((size_t)p.k * (size_t)p.N * 2);
		for (int i = 0; i < p.k; i++) {
			std::vector<uint8_t> t = serialize_poly_i16_centered(p, v.v[i]);
			common::bytes::append_bytes(out, t);
		}
		return out;
	}
	
	static std::vector<uint8_t> serialize_polymat_i16_centered(const common::Params& p,
		const lattice::PolyMat& A) {
		std::vector<uint8_t> out;
		out.reserve((size_t)p.k * (size_t)p.k * (size_t)p.N * 2);
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.k; j++) {
				std::vector<uint8_t> t = serialize_poly_i16_centered(p, A.m[i][j]);
				common::bytes::append_bytes(out, t);
			}
		}
		return out;
	}
	
	lattice::PolyMat DeriveCommitKey(const common::Params& p,
		const lattice::PublicKey& pk) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "Acom-v2");
		common::bytes::append_u64(buf, p.seed);
	
		common::bytes::append_u32(buf, (uint32_t)p.N);
		common::bytes::append_u32(buf, (uint32_t)p.q);
		common::bytes::append_u32(buf, (uint32_t)p.k);
		common::bytes::append_u32(buf, (uint32_t)p.eta);
		common::bytes::append_u32(buf, (uint32_t)p.B);
		common::bytes::append_u32(buf, (uint32_t)p.d);
	
		common::bytes::append_bytes(buf, serialize_polymat_i16_centered(p, pk.A));
		common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, pk.t));
	
		uint64_t seed = common::hash::hash64(buf);
		common::Rng rng(seed);
		return lattice::mat_uniform(p, rng);
	}
	
	lattice::PolyVec Commit(const common::Params& p,
		const lattice::PolyMat& Acom,
		const lattice::PolyVec& r_modq,
		const lattice::PolyVec& z_modq) {
		lattice::PolyVec rr = lattice::vec_mod_reduce(p, r_modq);
		lattice::PolyVec zz = lattice::vec_mod_reduce(p, z_modq);
		lattice::PolyVec Ar = lattice::mat_vec_mul(p, Acom, rr);
		return lattice::vec_add(p, Ar, zz);
	}

} 
