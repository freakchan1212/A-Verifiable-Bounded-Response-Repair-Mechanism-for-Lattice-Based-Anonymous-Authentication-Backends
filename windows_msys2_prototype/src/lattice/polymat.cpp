
#include "polymat.h"
#include "poly.h"
#include "ntt.h"
#include "mod_arith.h"

#include "common/bytes.h"
#include "common/hash.h"

#include <sstream>
#include <unordered_map>

namespace lattice {

	std::string PolyMat::to_string(int max_terms_per_poly) const {
		std::ostringstream oss;
		oss << "[\n";
		for (size_t i = 0; i < m.size(); i++) {
			oss << "  ";
			for (size_t j = 0; j < m[i].size(); j++) {
				oss << m[i][j].to_string(max_terms_per_poly);
				if (j + 1 < m[i].size()) oss << " ";
			}
			oss << "\n";
		}
		oss << "]";
		return oss.str();
	}
	
	PolyMat mat_uniform(const common::Params& p, common::Rng& rng) {
		PolyMat A(p.k, p.N);
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.k; j++) {
				A.m[(size_t)i][(size_t)j] = Poly::uniform(p, rng);
			}
		}
		return A;
	}
	
	
	
	
	
	static uint64_t mat_cache_key(const common::Params& p, const PolyMat& A) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "PolyMatNTTCache-v1");
		common::bytes::append_u32(buf, (uint32_t)p.N);
		common::bytes::append_u32(buf, (uint32_t)p.q);
		common::bytes::append_u32(buf, (uint32_t)p.k);
		common::bytes::append_u32(buf, (uint32_t)p.use_ntt);
	
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.k; j++) {
				for (int t = 0; t < p.N; t++) {
					common::bytes::append_i16(
						buf,
						(int16_t)center_q((int)A.m[(size_t)i][(size_t)j].c[(size_t)t], p.q)
					);
				}
			}
		}
		return common::hash::hash64(buf);
	}
	
	static PolyMat build_ntt_matrix(const common::Params& p, const PolyMat& A) {
		PolyMat F(p.k, p.N);
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.k; j++) {
				F.m[(size_t)i][(size_t)j] = A.m[(size_t)i][(size_t)j];
				lattice::ntt::forward(p, F.m[(size_t)i][(size_t)j]);
			}
		}
		return F;
	}
	
	static const PolyMat& get_cached_ntt_matrix(const common::Params& p, const PolyMat& A) {
		static std::unordered_map<uint64_t, PolyMat> cache;
	
		uint64_t key = mat_cache_key(p, A);
		auto it = cache.find(key);
		if (it != cache.end()) return it->second;
	
		PolyMat F = build_ntt_matrix(p, A);
		auto ins = cache.emplace(key, std::move(F));
		return ins.first->second;
	}
	
	static PolyVec mat_vec_mul_fallback_scalar(const common::Params& p, const PolyMat& A, const PolyVec& x) {
		PolyVec y = vec_zero(p);
		for (int i = 0; i < p.k; i++) {
			Poly acc = Poly::zero(p);
			for (int j = 0; j < p.k; j++) {
				Poly t = lattice::ntt::mul_ntt(p, A.m[(size_t)i][(size_t)j], x.v[(size_t)j]);
				acc = add(p, acc, t);
			}
			y.v[(size_t)i] = acc;
		}
		return y;
	}
	
	PolyVec mat_vec_mul(const common::Params& p, const PolyMat& A, const PolyVec& x) {
		
		if (!p.use_ntt || !lattice::ntt::supported(p) || p.offload_proto == 2) {
			return mat_vec_mul_fallback_scalar(p, A, x);
		}
	
		
		const PolyMat& A_ntt = get_cached_ntt_matrix(p, A);
	
		std::vector<Poly> x_ntt((size_t)p.k);
		for (int j = 0; j < p.k; j++) {
			x_ntt[(size_t)j] = x.v[(size_t)j];
			lattice::ntt::forward(p, x_ntt[(size_t)j]);
		}
	
		PolyVec y = vec_zero(p);
	
		for (int i = 0; i < p.k; i++) {
			Poly acc_freq = Poly::zero(p);
	
			for (int j = 0; j < p.k; j++) {
				Poly prod_freq(p.N);
				lattice::ntt::mul_freq(
					p,
					A_ntt.m[(size_t)i][(size_t)j],
					x_ntt[(size_t)j],
					prod_freq
				);
	
				for (int t = 0; t < p.N; t++) {
					int sum = (int)acc_freq.c[(size_t)t] + (int)prod_freq.c[(size_t)t];
					acc_freq.c[(size_t)t] = (Poly::coeff_t)modq(sum, p.q);
				}
			}
	
			lattice::ntt::inverse(p, acc_freq);
			y.v[(size_t)i] = acc_freq;
		}
	
		return y;
	}

} 
