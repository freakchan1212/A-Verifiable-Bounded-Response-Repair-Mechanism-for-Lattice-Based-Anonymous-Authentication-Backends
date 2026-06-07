
#include "offload/ntt_offload.h"

#include <vector>

#include "common/bytes.h"
#include "common/hash.h"
#include "common/log.h"

#include "lattice/ntt.h"
#include "lattice/mod_arith.h"

namespace offload {

	static std::vector<uint8_t> poly_digest(const common::Params& p, const lattice::Poly& a, const char* tag) {
		std::vector<uint8_t> buf;
		common::bytes::append_str(buf, tag);
		common::bytes::append_u64(buf, p.seed);
		for (int i = 0; i < p.N; i++) {
			common::bytes::append_i16(buf, (int16_t)lattice::center_q((int)a.c[(size_t)i], p.q));
		}
		return common::hash::hash32(buf);
	}
	
	static bool spotcheck_equal(const common::Params& p,
		const lattice::Poly& x,
		const lattice::Poly& y,
		common::Rng& rng,
		int checks) {
		if (checks <= 0) checks = 4;
		for (int t = 0; t < checks; t++) {
			int idx = rng.uniform_int(0, p.N - 1);
			if (lattice::modq((int)x.c[(size_t)idx], p.q) != lattice::modq((int)y.c[(size_t)idx], p.q)) return false;
		}
		return true;
	}
	
	static lattice::Poly local_mul_no_offload(const common::Params& p,
		const lattice::Poly& a,
		const lattice::Poly& b) {
		if (!p.use_ntt || !lattice::ntt::supported(p)) {
			return lattice::mul_schoolbook(p, a, b);
		}
	
		lattice::Poly A = a, B = b;
		lattice::ntt::forward(p, A);
		lattice::ntt::forward(p, B);
	
		lattice::Poly C(p.N);
		lattice::ntt::mul_freq(p, A, B, C);
		lattice::ntt::inverse(p, C);
		return C;
	}
	
	bool PolyMulOffload(const common::Params& p,
		const lattice::Poly& a,
		const lattice::Poly& b,
		lattice::Poly& c,
		common::Rng& rng,
		const AdversaryConfig& adv,
		NttOffloadStats* st) {
		if (st) st->n_calls++;
	
		if (!lattice::ntt::supported(p)) {
			c = lattice::mul_schoolbook(p, a, b);
			if (st) st->fallback_calls++;
			return false;
		}
	
		lattice::Poly A = a, B = b;
		lattice::Poly A_remote = a, B_remote = b;
	
		std::vector<uint8_t> sb;
		common::bytes::append_str(sb, "NTTOffload-v3");
		common::bytes::append_u64(sb, p.seed);
		common::bytes::append_u32(sb, (uint32_t)adv.straggler_pct);
		common::bytes::append_u32(sb, (uint32_t)adv.malicious_pct);
		common::bytes::append_bytes(sb, poly_digest(p, a, "a"));
		common::bytes::append_bytes(sb, poly_digest(p, b, "b"));
		uint64_t s0 = common::hash::hash64(sb);
		common::Rng r0(s0);
	
		
		lattice::ntt::forward(p, A_remote);
		lattice::ntt::forward(p, B_remote);
	
		auto maybe_corrupt = [&](lattice::Poly& P) {
			int dice = r0.uniform_int(0, 99);
			if (dice < adv.malicious_pct) {
				int idx = r0.uniform_int(0, p.N - 1);
				P.c[(size_t)idx] = (lattice::Poly::coeff_t)lattice::modq((int)P.c[(size_t)idx] + 1, p.q);
				return true;
			}
			return false;
		};
	
		bool corruptA = maybe_corrupt(A_remote);
		bool corruptB = maybe_corrupt(B_remote);
	
		
		lattice::ntt::forward(p, A);
		lattice::ntt::forward(p, B);
	
		common::Rng chk_rng(common::hash::hash64(poly_digest(p, a, "chkA")));
		bool okA = spotcheck_equal(p, A_remote, A, chk_rng, p.off_freivalds);
	
		common::Rng chk_rng2(common::hash::hash64(poly_digest(p, b, "chkB")));
		bool okB = spotcheck_equal(p, B_remote, B, chk_rng2, p.off_freivalds);
	
		if (!okA || !okB) {
			c = local_mul_no_offload(p, a, b);
			if (st) st->fallback_calls++;
			if (p.off_verbose) {
				LOGI("[NTTOffload] forward verify fail: okA=%d okB=%d corruptA=%d corruptB=%d -> fallback local",
					(int)okA, (int)okB, (int)corruptA, (int)corruptB);
			}
			return false;
		}
	
		
		lattice::Poly C_remote(p.N);
		lattice::ntt::mul_freq(p, A_remote, B_remote, C_remote);
	
		bool corruptC = maybe_corrupt(C_remote);
	
		lattice::Poly c_remote = C_remote;
		lattice::ntt::inverse(p, c_remote);
	
		
		lattice::Poly C_local(p.N);
		lattice::ntt::mul_freq(p, A, B, C_local);
		lattice::Poly c_local = C_local;
		lattice::ntt::inverse(p, c_local);
	
		common::Rng chk_rng3(common::hash::hash64(poly_digest(p, C_local, "chkInv")));
		bool okInv = spotcheck_equal(p, c_remote, c_local, chk_rng3, p.off_freivalds);
	
		if (!okInv) {
			c = c_local;
			if (st) st->fallback_calls++;
			if (p.off_verbose) {
				LOGI("[NTTOffload] inverse verify fail: corruptC=%d -> fallback local output", (int)corruptC);
			}
			return false;
		}
	
		c = c_remote;
		if (st) st->ok_calls++;
		if (p.off_verbose) {
			LOGI("[NTTOffload] PASS: corruptA=%d corruptB=%d corruptC=%d", (int)corruptA, (int)corruptB, (int)corruptC);
		}
		return true;
	}

} 
