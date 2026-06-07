
#include "zk/boundary_range.h"

#include <algorithm>

#include "common/bytes.h"
#include "common/hash.h"
#include "lattice/mod_arith.h"
#include "lattice/polyvec.h"

namespace zk {

	static inline int idx_of(const common::Params& p, int i, int j) {
		return i * p.N + j;
	}
	
	static void unflatten(const common::Params& p, uint32_t idx, int& i, int& j) {
		i = (int)(idx / (uint32_t)p.N);
		j = (int)(idx % (uint32_t)p.N);
	}
	
	uint64_t DigestBoundaryRangeProof(const BoundaryRangeProof& pf) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "BRangePf-v1");
		common::bytes::append_u32(buf, pf.L);
	
		common::bytes::append_u32(buf, (uint32_t)pf.idx.size());
		for (auto v : pf.idx) common::bytes::append_u32(buf, v);
	
		common::bytes::append_u32(buf, (uint32_t)pf.u_bits.size());
		for (auto b : pf.u_bits) buf.push_back((uint8_t)(b & 1));
	
		return common::hash::hash64(buf);
	}
	
	static std::vector<uint32_t> boundary_indices(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z) {
		std::vector<uint32_t> idx;
		idx.reserve((size_t)p.k * (size_t)p.N);
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = z.v[i].c[j]; 
				if (x == th.B || x == -th.B) idx.push_back((uint32_t)idx_of(p, i, j));
			}
		}
		std::sort(idx.begin(), idx.end());
		idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
		return idx;
	}
	
	BoundaryRangeProof ProveBoundaryRange(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z_star,
		const lattice::PolyVec& z,
		uint32_t L_bits) {
		BoundaryRangeProof pf;
		pf.L = L_bits;
		pf.idx = boundary_indices(p, th, z);
	
		pf.u_bits.clear();
		pf.u_bits.reserve((size_t)pf.idx.size() * (size_t)pf.L);
	
		for (uint32_t flat : pf.idx) {
			int i, j;
			unflatten(p, flat, i, j);
	
			int zcoef = z.v[i].c[j]; 
			int x = lattice::center_q(z_star.v[i].c[j], p.q); 
	
			int u = 0;
			if (zcoef == th.B) {
				u = x - th.B;              
				if (u < 0) u = 0;          
			}
			else {
				u = (-th.B) - x;           
				if (u < 0) u = 0;
			}
	
			
			for (uint32_t b = 0; b < pf.L; b++) {
				pf.u_bits.push_back((uint8_t)((u >> b) & 1));
			}
		}
		return pf;
	}
	
	bool VerifyBoundaryRange(const common::Params& p,
		const preauth::RepairTheta& th,
		const lattice::PolyVec& z,
		const BoundaryRangeProof& pf,
		std::vector<int>& out_x_centered) {
		
		std::vector<uint32_t> exp = boundary_indices(p, th, z);
		if (pf.idx != exp) return false;
	
		
		if (pf.L == 0 || pf.L > 24) return false;
		if (pf.u_bits.size() != (size_t)pf.idx.size() * (size_t)pf.L) return false;
	
		out_x_centered.clear();
		out_x_centered.reserve(pf.idx.size());
	
		
		for (size_t t = 0; t < pf.idx.size(); t++) {
			uint32_t flat = pf.idx[t];
			int i, j;
			unflatten(p, flat, i, j);
	
			int zcoef = z.v[i].c[j]; 
			if (!(zcoef == th.B || zcoef == -th.B)) return false;
	
			int u = 0;
			for (uint32_t b = 0; b < pf.L; b++) {
				uint8_t bit = pf.u_bits[t * pf.L + b] & 1;
				
				if (bit != 0 && bit != 1) return false;
				u |= ((int)bit << b);
			}
	
			int x = 0;
			if (zcoef == th.B) {
				x = th.B + u;
				if (x < th.B) return false;
			}
			else {
				x = -th.B - u;
				if (x > -th.B) return false;
			}
	
			
			
			out_x_centered.push_back(x);
		}
	
		return true;
	}
	
	lattice::PolyVec BuildY3_FromZ_AndBoundaryX(const common::Params& p,
		const preauth::RepairTheta& ,
		const lattice::PolyVec& z,
		const std::vector<uint32_t>& bidx,
		const std::vector<int>& x_centered) {
		
		lattice::PolyVec y3 = z;
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				y3.v[i].c[j] = lattice::modq(y3.v[i].c[j], p.q);
			}
		}
	
		
		for (size_t t = 0; t < bidx.size(); t++) {
			uint32_t flat = bidx[t];
			int i, j;
			unflatten(p, flat, i, j);
			y3.v[i].c[j] = lattice::modq(x_centered[t], p.q);
		}
	
		return y3;
	}

} 
