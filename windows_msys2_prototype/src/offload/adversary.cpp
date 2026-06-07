
#include "offload/adversary.h"
#include <algorithm>

namespace offload {

	AdversaryOutcome Simulate(int n_shards, const AdversaryConfig& cfg, common::Rng& rng) {
		AdversaryOutcome out;
		out.arrival_order.resize(n_shards);
		out.delivered.assign(n_shards, true);
		out.corrupted.assign(n_shards, false);
	
		for (int i = 0; i < n_shards; i++) out.arrival_order[i] = i;
	
		
		for (int i = n_shards - 1; i > 0; i--) {
			int j = rng.uniform_int(0, i);
			std::swap(out.arrival_order[i], out.arrival_order[j]);
		}
	
		
		for (int id = 0; id < n_shards; id++) {
			int r = rng.uniform_int(0, 99);
			if (r < cfg.straggler_pct) {
				out.delivered[id] = false;
				out.dropped++;
			}
		}
	
		
		for (int id = 0; id < n_shards; id++) {
			if (!out.delivered[id]) continue;
			int r = rng.uniform_int(0, 99);
			if (r < cfg.malicious_pct) {
				out.corrupted[id] = true;
				out.corrupted_cnt++;
			}
		}
	
		return out;
	}

} 
