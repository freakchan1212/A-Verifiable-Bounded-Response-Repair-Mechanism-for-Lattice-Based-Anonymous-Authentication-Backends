
#include "offload/matvec_offload_demo.h"

#include <iostream>

#include "common/log.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"

#include "offload/coded_offload.h"

namespace offload {

	void RunMatVecOffloadDemo(const common::Params& p, common::Rng& rng) {
		std::cout << "\n==============================\n"
			<< "Offload Demo (Coded-Offload + Freivalds verify)\n"
			<< "==============================\n";
	
		lattice::PolyMat A = lattice::mat_uniform(p, rng);
		lattice::PolyVec x = lattice::vec_uniform(p, rng);
	
		AdversaryConfig adv;
		adv.straggler_pct = p.off_straggler_pct;
		adv.malicious_pct = p.off_malicious_pct;
	
		lattice::PolyVec y;
		OffloadStats st;
		bool ok = CodedOffloadMatVec(p, A, x, y, st, rng, adv);
	
		std::cout << "[OffloadDemo] k=" << st.k << " r=" << st.r << " n=" << st.n
			<< " recv=" << st.received
			<< " drop=" << st.dropped
			<< " corrupt=" << st.corrupted
			<< " recovered=" << (st.recovered ? 1 : 0)
			<< " freivalds_ok=" << (st.freivalds_ok ? 1 : 0)
			<< " rounds=" << p.off_freivalds
			<< " -> " << (ok ? "PASS" : "FAIL")
			<< "\n";
	}

} 
