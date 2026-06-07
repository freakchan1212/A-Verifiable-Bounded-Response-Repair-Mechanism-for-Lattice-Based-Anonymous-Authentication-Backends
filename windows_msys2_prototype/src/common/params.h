#pragma once
#include <cstdint>
#include <string>

namespace common {

	struct Params {
		
		
		
		
		
		
		
		int preset = 3;
	
		
		int N = 256;
		int q = 12289;
		int k = 3;
		int eta = 2;
	
		
		int B = 6;
		int d = 3;
	
		
		uint64_t seed = 0xC0FFEE1234ULL;
	
		
		int use_ntt = 1;
	
		
		int zk_Lbits = 8;
		int zk_audit_t = 16;
		int smoothing_eta = 2;
		
		int zk_backend = 2;
	
		
		int off_k = 2;
		int off_r = 1;
		int freivalds_rounds = 8;
	
		int off_freivalds = 8;
		int offload_demo = 0;
		int offload_proto = 0;
	
		int off_straggler_pct = 0;
		int off_malicious_pct = 0;
		int off_verbose = 1;
	
		
		int off_threaded = 0;
		int off_workers = 1;
		int off_delay_us = 0;
		int off_delay_jitter_us = 0;
		int off_delay_step_us = 0;
	
		
		int off_sweep = 0;
		int off_sweep_trials = 200;
	
		int trials = 100;
	};
	
	void ApplyPreset(Params& p, int preset_id);
	Params DefaultParams();
	bool ParseArgs(int argc, char** argv, Params& p);
	std::string ToString(const Params& p);

} 
