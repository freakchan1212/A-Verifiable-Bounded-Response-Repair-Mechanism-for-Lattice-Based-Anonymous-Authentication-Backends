#include "commit.h"
#include "common/bytes.h"
#include "common/hash.h"
#include "lattice/mod_arith.h"

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
	
	std::vector<uint8_t> commit_hash(const common::Params& p, const CommitMsg& cm) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, "Commit-v2");
		common::bytes::append_u32(buf, cm.nonce);
		common::bytes::append_u64(buf, cm.scope_tag);
		common::bytes::append_u64(buf, cm.pseudonym);
		common::bytes::append_u64(buf, cm.refresh_tag);
		common::bytes::append_bytes(buf, serialize_polyvec_i16(p, cm.w));
		return common::hash::hash32(buf);
	}

} 
