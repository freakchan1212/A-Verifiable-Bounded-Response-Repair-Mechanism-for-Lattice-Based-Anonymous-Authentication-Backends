
#pragma once
#include <vector>
#include "common/rng.h"

namespace offload {

	struct AdversaryConfig {
		int straggler_pct = 0; 
		int malicious_pct = 0; 
	};
	
	struct AdversaryOutcome {
		std::vector<int> arrival_order;  
		std::vector<bool> delivered;     
		std::vector<bool> corrupted;     
		int dropped = 0;
		int corrupted_cnt = 0;
	};
	
	AdversaryOutcome Simulate(int n_shards, const AdversaryConfig& cfg, common::Rng& rng);

} 
