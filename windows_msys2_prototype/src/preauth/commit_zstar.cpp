
#include "commit_zstar.h"
#include "common/bytes.h"
#include "common/hash.h"
#include "lattice/mod_arith.h"

namespace preauth {

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
	
	std::vector<uint8_t> commit_zstar(const common::Params& p, const lattice::PolyVec& z_star) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "ZSTAR-v1");
		std::vector<uint8_t> zbytes = serialize_polyvec_i16_centered(p, z_star);
		common::bytes::append_bytes(buf, zbytes);
		return common::hash::hash32(buf);
	}

} 
