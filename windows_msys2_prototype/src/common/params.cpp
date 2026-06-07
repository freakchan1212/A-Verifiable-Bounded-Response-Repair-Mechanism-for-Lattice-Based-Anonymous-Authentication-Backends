
#include "params.h"
#include "util.h"

#include <sstream>
#include <cstring>
#include <cstdlib>

namespace common {

	static bool match(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
	
	static bool read_int(int argc, char** argv, int& i, int& out) {
		if (i + 1 >= argc) return false;
		out = std::atoi(argv[i + 1]);
		i += 1;
		return true;
	}
	
	static bool read_u64(int argc, char** argv, int& i, uint64_t& out) {
		if (i + 1 >= argc) return false;
		out = (uint64_t)std::strtoull(argv[i + 1], nullptr, 10);
		i += 1;
		return true;
	}
	
	void ApplyPreset(Params& p, int preset_id) {
		p.preset = preset_id;
	
		switch (preset_id) {
		case 1: 
			p.N = 16;
			p.q = 3329;
			p.k = 2;
			p.eta = 2;
			p.B = 8;
			p.d = 3;
			p.use_ntt = 0;
	
			p.zk_Lbits = 6;
			p.zk_audit_t = 8;
			p.smoothing_eta = 1;
	
			p.off_k = 2;
			p.off_r = 1;
			p.freivalds_rounds = 8;
			p.off_freivalds = 8;
			break;
	
		case 2: 
			p.N = 64;
			p.q = 12289;
			p.k = 2;
			p.eta = 2;
			p.B = 8;
			p.d = 3;
			p.use_ntt = 1;
	
			p.zk_Lbits = 8;
			p.zk_audit_t = 12;
			p.smoothing_eta = 2;
	
			p.off_k = 2;
			p.off_r = 1;
			p.freivalds_rounds = 12;
			p.off_freivalds = 12;
			break;
	
		case 3: 
			p.N = 256;
			p.q = 12289;
			p.k = 3;
			p.eta = 2;
			p.B = 6;
			p.d = 3;
			p.use_ntt = 1;
	
			p.zk_Lbits = 10;
			p.zk_audit_t = 16;
			p.smoothing_eta = 2;
	
			p.off_k = 3;
			p.off_r = 1;
			p.freivalds_rounds = 16;
			p.off_freivalds = 16;
			break;
	
		case 4: 
			
			
			p.N = 256;
			p.q = 3329;
			p.k = 3;
			p.eta = 2;
			p.B = 6;
			p.d = 3;
			p.use_ntt = 1;
	
			p.zk_Lbits = 10;
			p.zk_audit_t = 16;
			p.smoothing_eta = 2;
	
			p.off_k = 3;
			p.off_r = 1;
			p.freivalds_rounds = 16;
			p.off_freivalds = 16;
			break;
	
		case 5: 
			p.N = 256;
			p.q = 3329;
			p.k = 4;
			p.eta = 2;
			p.B = 6;
			p.d = 3;
			p.use_ntt = 1;
	
			p.zk_Lbits = 12;
			p.zk_audit_t = 24;
			p.smoothing_eta = 2;
	
			p.off_k = 4;
			p.off_r = 1;
			p.freivalds_rounds = 24;
			p.off_freivalds = 24;
			break;
	
		default:
			p.preset = 0;
			break;
		}
	}
	
	Params DefaultParams() {
		Params p{};
		ApplyPreset(p, 3);
		return p;
	}
	
	bool ParseArgs(int argc, char** argv, Params& p) {
		
		for (int i = 1; i < argc; i++) {
			if (match(argv[i], "--preset")) {
				int v = 0;
				if (!read_int(argc, argv, i, v)) return false;
				ApplyPreset(p, v);
			}
		}
	
		
		for (int i = 1; i < argc; i++) {
			if (match(argv[i], "--preset")) {
				int dummy = 0;
				if (!read_int(argc, argv, i, dummy)) return false;
				continue;
			}
	
			if (match(argv[i], "--N")) { if (!read_int(argc, argv, i, p.N)) return false; continue; }
			if (match(argv[i], "--q")) { if (!read_int(argc, argv, i, p.q)) return false; continue; }
			if (match(argv[i], "--k")) { if (!read_int(argc, argv, i, p.k)) return false; continue; }
			if (match(argv[i], "--eta")) { if (!read_int(argc, argv, i, p.eta)) return false; continue; }
	
			if (match(argv[i], "--B")) { if (!read_int(argc, argv, i, p.B)) return false; continue; }
			if (match(argv[i], "--d")) { if (!read_int(argc, argv, i, p.d)) return false; continue; }
	
			if (match(argv[i], "--seed")) { if (!read_u64(argc, argv, i, p.seed)) return false; continue; }
			if (match(argv[i], "--trials")) { if (!read_int(argc, argv, i, p.trials)) return false; continue; }
	
			if (match(argv[i], "--use_ntt")) { if (!read_int(argc, argv, i, p.use_ntt)) return false; continue; }
	
			if (match(argv[i], "--zk_Lbits")) { if (!read_int(argc, argv, i, p.zk_Lbits)) return false; continue; }
			if (match(argv[i], "--zk_audit_t")) { if (!read_int(argc, argv, i, p.zk_audit_t)) return false; continue; }
			if (match(argv[i], "--smoothing_eta")) { if (!read_int(argc, argv, i, p.smoothing_eta)) return false; continue; }
			if (match(argv[i], "--zk_backend")) { if (!read_int(argc, argv, i, p.zk_backend)) return false; continue; }
	
			if (match(argv[i], "--off_k")) { if (!read_int(argc, argv, i, p.off_k)) return false; continue; }
			if (match(argv[i], "--off_r")) { if (!read_int(argc, argv, i, p.off_r)) return false; continue; }
			if (match(argv[i], "--freivalds")) { if (!read_int(argc, argv, i, p.freivalds_rounds)) return false; continue; }
	
			if (match(argv[i], "--off_freivalds")) { if (!read_int(argc, argv, i, p.off_freivalds)) return false; continue; }
			if (match(argv[i], "--offload_demo")) { if (!read_int(argc, argv, i, p.offload_demo)) return false; continue; }
			if (match(argv[i], "--offload_proto")) { if (!read_int(argc, argv, i, p.offload_proto)) return false; continue; }
			if (match(argv[i], "--off_straggler")) { if (!read_int(argc, argv, i, p.off_straggler_pct)) return false; continue; }
			if (match(argv[i], "--off_malicious")) { if (!read_int(argc, argv, i, p.off_malicious_pct)) return false; continue; }
			if (match(argv[i], "--off_verbose")) { if (!read_int(argc, argv, i, p.off_verbose)) return false; continue; }
	
			if (match(argv[i], "--off_threaded")) { if (!read_int(argc, argv, i, p.off_threaded)) return false; continue; }
			if (match(argv[i], "--off_workers")) { if (!read_int(argc, argv, i, p.off_workers)) return false; continue; }
			if (match(argv[i], "--off_delay_us")) { if (!read_int(argc, argv, i, p.off_delay_us)) return false; continue; }
			if (match(argv[i], "--off_delay_jitter_us")) { if (!read_int(argc, argv, i, p.off_delay_jitter_us)) return false; continue; }
			if (match(argv[i], "--off_delay_step_us")) { if (!read_int(argc, argv, i, p.off_delay_step_us)) return false; continue; }
	
			if (match(argv[i], "--off_sweep")) { if (!read_int(argc, argv, i, p.off_sweep)) return false; continue; }
			if (match(argv[i], "--off_sweep_trials")) { if (!read_int(argc, argv, i, p.off_sweep_trials)) return false; continue; }
	
			
		}
		return true;
	}
	
	std::string ToString(const Params& p) {
		std::ostringstream oss;
		oss << "Params{"
			<< "preset=" << p.preset
			<< ", N=" << p.N
			<< ", q=" << p.q
			<< ", k=" << p.k
			<< ", eta=" << p.eta
			<< ", B=" << p.B
			<< ", d=" << p.d
			<< ", seed=" << hex_u64(p.seed)
			<< ", use_ntt=" << p.use_ntt
			<< ", zk_Lbits=" << p.zk_Lbits
			<< ", zk_audit_t=" << p.zk_audit_t
			<< ", smoothing_eta=" << p.smoothing_eta
			<< ", zk_backend=" << p.zk_backend
			<< ", off_k=" << p.off_k
			<< ", off_r=" << p.off_r
			<< ", freivalds=" << p.freivalds_rounds
			<< ", off_freivalds=" << p.off_freivalds
			<< ", offload_demo=" << p.offload_demo
			<< ", offload_proto=" << p.offload_proto
			<< ", off_straggler=" << p.off_straggler_pct
			<< ", off_malicious=" << p.off_malicious_pct
			<< ", off_threaded=" << p.off_threaded
			<< ", off_workers=" << p.off_workers
			<< ", off_delay_us=" << p.off_delay_us
			<< ", off_delay_jitter_us=" << p.off_delay_jitter_us
			<< ", off_delay_step_us=" << p.off_delay_step_us
			<< ", off_sweep=" << p.off_sweep
			<< ", off_sweep_trials=" << p.off_sweep_trials
			<< ", trials=" << p.trials
			<< "}";
		return oss.str();
	}

} 
