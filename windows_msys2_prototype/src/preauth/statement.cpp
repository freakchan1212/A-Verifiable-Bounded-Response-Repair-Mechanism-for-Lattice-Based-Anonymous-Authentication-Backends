#include "statement.h"
#include "lattice/mod_arith.h"
#include "lattice/polyvec.h"
#include "preauth/repair.h"

namespace preauth::stmt {

	static bool polyvec_equal_centered(const common::Params& p,
		const lattice::PolyVec& a,
		const lattice::PolyVec& b) {
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int xa = lattice::center_q((int)a.v[(size_t)i].c[(size_t)j], p.q);
				int xb = lattice::center_q((int)b.v[(size_t)i].c[(size_t)j], p.q);
				if (xa != xb) return false;
			}
		}
		return true;
	}
	
	bool CheckRepairRelation(const common::Params& p,
		const RepairTheta& th,
		const lattice::PolyVec& z_star,
		const lattice::PolyVec& z,
		const RepairMeta& meta) {
		RepairResult rr = repair_polyvec(p, th, z_star);
		
		if (!polyvec_equal_centered(p, rr.z, z)) return false;
		if (rr.meta.high.size() != meta.high.size()) return false;
		if (rr.meta.low.size() != meta.low.size()) return false;
		for (size_t i = 0; i < rr.meta.high.size(); i++) {
			if (rr.meta.high[i] != meta.high[i]) return false;
		}
		for (size_t i = 0; i < rr.meta.low.size(); i++) {
			if (rr.meta.low[i] != meta.low[i]) return false;
		}
		if (rr.meta.clamp_count != meta.clamp_count) return false;
		if (rr.meta.delta_l0 != meta.delta_l0) return false;
		if (rr.meta.delta_linf != meta.delta_linf) return false;
		if (rr.meta.delta_digest != meta.delta_digest) return false;
		return check_repair(p, th, z, meta);
	}
	
	bool CheckResidualSmallCentered(const common::Params& p,
		const lattice::PolyVec& r_small) {
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				int x = r_small.v[(size_t)i].c[(size_t)j];
				x = lattice::center_q(x, p.q);
				if (x > p.eta || x < -p.eta) return false;
			}
		}
		return true;
	}
	
	lattice::PolyVec BuildYFromCommit(const common::Params& p,
		const lattice::PublicKey& pk,
		const CommitMsg& cm,
		int c,
		const lattice::PolyVec& r_small) {
		lattice::PolyVec y = cm.w;
		if (c & 1) {
			y = lattice::vec_add(p, y, pk.t);
		}
		lattice::PolyVec r_mod = lattice::vec_mod_reduce(p, r_small);
		y = lattice::vec_add(p, y, r_mod);
		return y;
	}

} 
