#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <ctime>

#ifdef _WIN32
#include <direct.h>
static void ensure_dir(const char* name) { _mkdir(name); }
#else
#include <sys/stat.h>
#include <sys/types.h>
static void ensure_dir(const char* name) { mkdir(name, 0755); }
#endif

#include "common/params.h"
#include "common/log.h"
#include "common/rng.h"
#include "common/timing.h"
#include "common/util.h"

#include "lattice/module_lwe.h"
#include "lattice/poly.h"
#include "lattice/polymat.h"
#include "lattice/ntt.h"
#include "lattice/mod_arith.h"

#include "preauth/preauth.h"
#include "offload/sweep.h"


void run_exp_A1A2_retry_budget_sweep(const common::Params& base, int sessions);
namespace exp_boundary_sweep {
	void run_exp_A1A2_boundary_sweep(const common::Params& base, int sessions, int max_attempts);
}
namespace review_supp {
	void run_exp_review_supp(const common::Params& base, int review_seeds, int review_sessions, int review_trials,
		int do_multiseed, int do_accept_prob, int do_metadata_leak, int do_context_mismatch);
}


static std::string now_ymdhms() {
	std::time_t t = std::time(nullptr);
	std::tm tmv;
#ifdef _WIN32
	localtime_s(&tmv, &t);
#else
	tmv = *std::localtime(&t);
#endif
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
		tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
		tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
	return std::string(buf);
}

static std::string hex_noprefix_u64(uint64_t x) {
	std::ostringstream oss;
	oss << std::hex << std::nouppercase << x;
	return oss.str();
}

static int get_arg_int(int argc, char** argv, const std::string& name, int defv) {
	for (int i = 1; i + 1 < argc; i++) {
		if (name == argv[i]) return std::atoi(argv[i + 1]);
	}
	return defv;
}

static void print_preset_guide() {
	std::cout
		<< "[PRESETS]\n"
		<< "  --preset 1 : toy\n"
		<< "  --preset 2 : stage1_64_ntt\n"
		<< "  --preset 3 : paper_ntt_256 (default)\n"
		<< "  --preset 4 : deploy_mlkem768_like\n"
		<< "  --preset 5 : deploy_mlkem1024_like\n";
}

static void run_exp_A1A2_abort_vs_repair(const common::Params& base, int sessions, int max_attempts) {
	ensure_dir("data");
	const std::string ts = now_ymdhms();
	const char* xof = "shake256";

	struct Case { int B; int eta; };
	std::vector<Case> cases = {
		{8, 2},
		{4, 4},
		{3, 6},
	};
	
	std::ostringstream fn;
	fn << "data/exp_A1A2_abort_vs_repair_" << ts
		<< "_xof" << xof
		<< "_N" << base.N << "_k" << base.k << "_q" << base.q
		<< "_sessions" << sessions
		<< "_seed" << hex_noprefix_u64(base.seed)
		<< ".csv";
	
	std::ofstream ofs(fn.str().c_str(), std::ios::out | std::ios::trunc);
	if (!ofs) { std::cout << "cannot open " << fn.str() << "\n"; return; }
	
	ofs << "case_id,B,eta,mode,session_idx,attempts,ok,total_ms,prove_ms_sum,verify_ms,"
		"zstar_maxabs_centered,clamp_count,audit_cnt,c_scalar\n";
	
	const uint64_t scope = 0x1111ULL;
	
	for (int ci = 0; ci < (int)cases.size(); ci++) {
		common::Params p = base;
		p.offload_proto = 0;
		p.off_delay_us = 0;
		p.off_delay_jitter_us = 0;
		p.off_delay_step_us = 0;
	
		p.B = cases[ci].B;
		p.eta = cases[ci].eta;
	
		common::Rng krng(p.seed ^ (uint64_t)ci * 0x9E3779B97F4A7C15ULL);
		lattice::KeyPair kp = lattice::keygen(p, krng);
	
		
		{
			common::Rng rng(p.seed ^ 0xA1A2B1B2ULL ^ (uint64_t)ci);
			preauth::Prover P(p, kp.pk, kp.sk);
			preauth::Verifier V(p, kp.pk);
			P.set_scope(scope);
			V.set_scope(scope);
	
			for (int si = 0; si < sessions; si++) {
				common::Timer tt;
	
				preauth::ProveObs obs;
				common::Timer tp;
				preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);
				double prove_ms = tp.ms();
	
				common::Timer tv;
				bool ok = V.verify_nizk(pf);
				double verify_ms = tv.ms();
	
				double total_ms = tt.ms();
	
				if (ok) P.on_accept(pf.cm, pf.rsp);
	
				ofs << ci << "," << p.B << "," << p.eta << ",repair,"
					<< si << "," << 1 << "," << (ok ? 1 : 0) << ","
					<< total_ms << "," << prove_ms << "," << verify_ms << ","
					<< obs.zstar_maxabs_centered << "," << obs.clamp_count << ","
					<< (uint64_t)obs.audit_cnt << "," << obs.c_scalar << "\n";
			}
		}
	
		
		{
			common::Rng rng(p.seed ^ 0xBADC0FFEEULL ^ (uint64_t)ci);
			preauth::Prover P(p, kp.pk, kp.sk);
			preauth::Verifier V(p, kp.pk);
			P.set_scope(scope);
			V.set_scope(scope);
	
			for (int si = 0; si < sessions; si++) {
				int attempts = 0;
				bool ok = false;
				double prove_sum = 0.0;
				double verify_ms = 0.0;
				double total_ms = 0.0;
	
				preauth::ProveObs obs_last;
				preauth::AuthProof pf_last;
	
				common::Timer t_session;
	
				while (attempts < max_attempts) {
					attempts++;
	
					preauth::ProveObs obs;
					common::Timer tp;
					preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);
					double prove_ms = tp.ms();
					prove_sum += prove_ms;
	
					if (obs.zstar_maxabs_centered > p.B) {
						continue;
					}
	
					common::Timer tv;
					ok = V.verify_nizk(pf);
					verify_ms = tv.ms();
	
					pf_last = pf;
					obs_last = obs;
	
					if (ok) P.on_accept(pf.cm, pf.rsp);
					break;
				}
	
				total_ms = t_session.ms();
	
				ofs << ci << "," << p.B << "," << p.eta << ",abort_baseline,"
					<< si << "," << attempts << "," << (ok ? 1 : 0) << ","
					<< total_ms << "," << prove_sum << "," << verify_ms << ","
					<< obs_last.zstar_maxabs_centered << "," << obs_last.clamp_count << ","
					<< (uint64_t)obs_last.audit_cnt << "," << obs_last.c_scalar << "\n";
			}
		}
	}
	
	ofs.flush();
	std::cout << "[EXP] wrote: " << fn.str() << "\n";
}

static void run_exp_B1_tamper(const common::Params& base, int tamper_trials) {
	ensure_dir("data");
	const std::string ts = now_ymdhms();
	const char* xof = "shake256";

	common::Params p = base;
	p.offload_proto = 0;
	p.off_delay_us = 0;
	p.off_delay_jitter_us = 0;
	p.off_delay_step_us = 0;
	p.B = 3;
	p.eta = 6;
	
	common::Rng krng(p.seed ^ 0xB1B1B1B1ULL);
	lattice::KeyPair kp = lattice::keygen(p, krng);
	
	const uint64_t scope = 0x1111ULL;
	
	std::ostringstream fn;
	fn << "data/exp_B1_tamper_" << ts
		<< "_xof" << xof
		<< "_N" << p.N << "_k" << p.k << "_q" << p.q
		<< "_B" << p.B << "_eta" << p.eta
		<< "_tr" << tamper_trials
		<< "_seed" << hex_noprefix_u64(p.seed)
		<< ".csv";
	
	std::ofstream ofs(fn.str().c_str(), std::ios::out | std::ios::trunc);
	if (!ofs) { std::cout << "cannot open " << fn.str() << "\n"; return; }
	
	ofs << "test,trial,expected_fail,verify_ok\n";
	
	common::Rng rng(p.seed ^ 0xC0FFEEULL);
	preauth::Prover P(p, kp.pk, kp.sk);
	preauth::Verifier V(p, kp.pk);
	P.set_scope(scope);
	V.set_scope(scope);
	
	common::Params p2 = p;
	p2.off_k = p.off_k + 1;
	preauth::Verifier V_ctx(p2, kp.pk);
	V_ctx.set_scope(scope);
	
	for (int i = 0; i < tamper_trials; i++) {
		preauth::ProveObs obs;
		preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);
	
		bool ok0 = V.verify_nizk(pf);
		if (ok0) P.on_accept(pf.cm, pf.rsp);
	
		ofs << "valid," << i << "," << 0 << "," << (ok0 ? 1 : 0) << "\n";
	
		{
			preauth::AuthProof pf2 = pf;
			pf2.rsp.z.v[0].c[0] = (lattice::Poly::coeff_t)((int)pf2.rsp.z.v[0].c[0] + 1);
			bool ok = V.verify_nizk(pf2);
			ofs << "tamper_z," << i << "," << 1 << "," << (ok ? 1 : 0) << "\n";
		}
	
		{
			preauth::AuthProof pf2 = pf;
			if (!pf2.rsp.meta.high.empty()) pf2.rsp.meta.high[0] = (int16_t)(pf2.rsp.meta.high[0] + 1);
			else pf2.rsp.meta.clamp_count += 1;
			bool ok = V.verify_nizk(pf2);
			ofs << "tamper_meta," << i << "," << 1 << "," << (ok ? 1 : 0) << "\n";
		}
	
		{
			preauth::AuthProof pf2 = pf;
			pf2.cm.scope_tag ^= 1ULL;
			bool ok = V.verify_nizk(pf2);
			ofs << "tamper_scope," << i << "," << 1 << "," << (ok ? 1 : 0) << "\n";
		}
	
		{
			bool ok = V_ctx.verify_nizk(pf);
			ofs << "tamper_ctx_verify_with_changed_params," << i << "," << 1 << "," << (ok ? 1 : 0) << "\n";
		}
	}
	
	ofs.flush();
	std::cout << "[EXP] wrote: " << fn.str() << "\n";
}

static void run_exp_B2_refresh(const common::Params& base, int sessions) {
	ensure_dir("data");
	const std::string ts = now_ymdhms();
	const char* xof = "shake256";

	common::Params p = base;
	p.offload_proto = 0;
	p.off_delay_us = 0;
	p.off_delay_jitter_us = 0;
	p.off_delay_step_us = 0;
	p.B = 3;
	p.eta = 6;
	
	common::Rng krng(p.seed ^ 0xB2B2B2B2ULL);
	lattice::KeyPair kp = lattice::keygen(p, krng);
	
	const uint64_t scope = 0x1111ULL;
	
	std::ostringstream fn;
	fn << "data/exp_B2_refresh_" << ts
		<< "_xof" << xof
		<< "_N" << p.N << "_k" << p.k << "_q" << p.q
		<< "_B" << p.B << "_eta" << p.eta
		<< "_sessions" << sessions
		<< "_seed" << hex_noprefix_u64(p.seed)
		<< ".csv";
	
	std::ofstream ofs(fn.str().c_str(), std::ios::out | std::ios::trunc);
	if (!ofs) { std::cout << "cannot open " << fn.str() << "\n"; return; }
	
	ofs << "session,verify_ok,c_scalar,audit_cnt,clamp_count,zstar_maxabs,"
		"prover_epoch,prover_leak,verifier_epoch,verifier_leak,refresh_tag\n";
	
	common::Rng rng(p.seed ^ 0xFEEDB2ULL);
	preauth::Prover P(p, kp.pk, kp.sk);
	preauth::Verifier V(p, kp.pk);
	P.set_scope(scope);
	V.set_scope(scope);
	
	for (int si = 0; si < sessions; si++) {
		preauth::ProveObs obs;
		preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);
	
		bool ok = V.verify_nizk(pf);
		if (ok) P.on_accept(pf.cm, pf.rsp);
	
		uint32_t v_epoch = V.has_member(pf.cm.pseudonym) ? V.member_epoch(pf.cm.pseudonym) : 0;
		size_t v_leak = V.has_member(pf.cm.pseudonym) ? V.member_leak_accum(pf.cm.pseudonym) : 0;
	
		ofs << si << "," << (ok ? 1 : 0) << ","
			<< obs.c_scalar << "," << (uint64_t)obs.audit_cnt << ","
			<< obs.clamp_count << "," << obs.zstar_maxabs_centered << ","
			<< P.epoch() << "," << (uint64_t)P.leak_accum() << ","
			<< v_epoch << "," << (uint64_t)v_leak << ","
			<< "0x" << hex_noprefix_u64(pf.cm.refresh_tag) << "\n";
	}
	
	ofs.flush();
	std::cout << "[EXP] wrote: " << fn.str() << "\n";
}

static bool poly_equal_modq(const common::Params& p, const lattice::Poly& a, const lattice::Poly& b) {
	for (int i = 0; i < p.N; i++) {
		if (lattice::modq((int)a.c[(size_t)i], p.q) != lattice::modq((int)b.c[(size_t)i], p.q)) return false;
	}
	return true;
}

static int run_selftest_ntt(const common::Params& base, int trials) {
	common::Params p = base;
	p.offload_proto = 0;
	p.use_ntt = 1;

	std::cout << "[SELFTEST] NTT polynomial/matrix consistency\n";
	std::cout << common::ToString(p) << "\n";
	
	if (!lattice::ntt::supported(p)) {
		std::cout << "[SELFTEST] NTT not supported for this parameter set.\n";
		return 1;
	}
	
	common::Rng rng(p.seed ^ 0x5E1F7E57ULL);
	
	
	for (int t = 0; t < trials; t++) {
		lattice::Poly a = lattice::Poly::small(p, rng);
		lattice::Poly b = lattice::Poly::small(p, rng);
	
		lattice::Poly c0 = lattice::mul_schoolbook(p, a, b);
		lattice::Poly c1 = lattice::ntt::mul_ntt(p, a, b);
	
		if (!poly_equal_modq(p, c0, c1)) {
			std::cout << "[SELFTEST] poly mul mismatch at trial " << t << "\n";
			std::cout << "schoolbook=" << c0.to_string(16) << "\n";
			std::cout << "ntt      =" << c1.to_string(16) << "\n";
			return 2;
		}
	}
	
	
	for (int t = 0; t < trials; t++) {
		lattice::PolyMat A = lattice::mat_uniform(p, rng);
		lattice::PolyVec x = lattice::vec_small(p, rng);
	
		common::Params p_fast = p;
		p_fast.use_ntt = 1;
		p_fast.offload_proto = 0;
	
		common::Params p_ref = p;
		p_ref.use_ntt = 0;
		p_ref.offload_proto = 0;
	
		lattice::PolyVec y_fast = lattice::mat_vec_mul(p_fast, A, x);
		lattice::PolyVec y_ref = lattice::mat_vec_mul(p_ref, A, x);
	
		bool ok = true;
		for (int i = 0; i < p.k && ok; i++) {
			if (!poly_equal_modq(p, y_fast.v[(size_t)i], y_ref.v[(size_t)i])) ok = false;
		}
	
		if (!ok) {
			std::cout << "[SELFTEST] mat-vec mismatch at trial " << t << "\n";
			return 3;
		}
	}
	
	std::cout << "[SELFTEST] PASS (" << trials << " trials)\n";
	return 0;
}


void run_exp_C1_overhead_and_size(const common::Params& base, int sessions);
void run_exp_D1_undetected_vs_freivalds(const common::Params& base, int trials);
void run_exp_D5_delay_compare(const common::Params& base, int trials);

static void run_exp_standardized_suite(
	const common::Params& base,
	int include_stage1,
	int include_mlkem1024,
	int sessions,
	int tamper_trials,
	int refresh_sessions,
	int c1_sessions,
	int d1_trials,
	int d5_trials,
	int max_attempts
) {
	std::vector<int> presets;
	if (include_stage1) presets.push_back(2);
	presets.push_back(3);
	presets.push_back(4);
	if (include_mlkem1024) presets.push_back(5);

	std::cout << "[MODE] exp_standardized=1\n";
	std::cout << "[INFO] This will run standardized suites over selected presets.\n";
	print_preset_guide();
	
	for (int preset_id : presets) {
		common::Params p = base;
		common::ApplyPreset(p, preset_id);
	
		
		p.seed = base.seed;
	
		std::cout << "\n[STD] running preset=" << preset_id << "\n";
		std::cout << common::ToString(p) << "\n";
	
		run_exp_A1A2_abort_vs_repair(p, sessions, max_attempts);
		run_exp_B1_tamper(p, tamper_trials);
		run_exp_B2_refresh(p, refresh_sessions);
		run_exp_C1_overhead_and_size(p, c1_sessions);
		run_exp_D1_undetected_vs_freivalds(p, d1_trials);
		run_exp_D5_delay_compare(p, d5_trials);
	
		std::cout << "[STD] preset=" << preset_id << " done.\n";
	}
	
	std::cout << "\n[STD] all selected presets done.\n";
}

int main(int argc, char** argv) {
	common::Params params = common::DefaultParams();
	if (!common::ParseArgs(argc, argv, params)) {
		LOGE("ParseArgs failed.");
		return 1;
	}

	const int selftest_ntt = get_arg_int(argc, argv, "--selftest_ntt", 0);
	if (selftest_ntt) {
		const int trials = get_arg_int(argc, argv, "--selftest_trials", 100);
		return run_selftest_ntt(params, trials);
	}
	
	const int show_presets = get_arg_int(argc, argv, "--show_presets", 0);
	if (show_presets) {
		print_preset_guide();
		std::cout << common::ToString(params) << "\n";
		return 0;
	}
	
	
	const int exp_standardized = get_arg_int(argc, argv, "--exp_standardized", 0);
	if (exp_standardized) {
		const int include_stage1 = get_arg_int(argc, argv, "--std_include_stage1", 0);
		const int include_mlkem1024 = get_arg_int(argc, argv, "--std_include_mlkem1024", 0);
	
		const int sessions = get_arg_int(argc, argv, "--std_sessions", 100);
		const int tamper_trials = get_arg_int(argc, argv, "--std_tamper", 30);
		const int refresh_sessions = get_arg_int(argc, argv, "--std_refresh", 100);
		const int c1_sessions = get_arg_int(argc, argv, "--std_c1", 100);
		const int d1_trials = get_arg_int(argc, argv, "--std_d1", 200);
		const int d5_trials = get_arg_int(argc, argv, "--std_d5", 100);
		const int max_attempts = get_arg_int(argc, argv, "--std_max_attempts", 2000);
	
		run_exp_standardized_suite(
			params,
			include_stage1,
			include_mlkem1024,
			sessions,
			tamper_trials,
			refresh_sessions,
			c1_sessions,
			d1_trials,
			d5_trials,
			max_attempts
		);
		return 0;
	}
	
	
	const int exp_review_supp = get_arg_int(argc, argv, "--exp_review_supp", 0);
	if (exp_review_supp) {
		const int review_seeds = get_arg_int(argc, argv, "--review_seeds", 5);
		const int review_sessions = get_arg_int(argc, argv, "--review_sessions", 200);
		const int review_trials = get_arg_int(argc, argv, "--review_trials", 3000);
		const int exp_multiseed = get_arg_int(argc, argv, "--exp_multiseed", 0);
		const int exp_accept_prob = get_arg_int(argc, argv, "--exp_accept_prob", 0);
		const int exp_metadata_leak = get_arg_int(argc, argv, "--exp_metadata_leak", 0);
		const int exp_context_mismatch = get_arg_int(argc, argv, "--exp_context_mismatch", 0);

		std::cout << "[MODE] exp_review_supp=1\n";
		std::cout << common::ToString(params) << "\n";
		review_supp::run_exp_review_supp(params, review_seeds, review_sessions, review_trials,
			exp_multiseed, exp_accept_prob, exp_metadata_leak, exp_context_mismatch);
		std::cout << "[REVIEW] supplementary experiments done.\n";
		return 0;
	}

	
	if (params.off_sweep) {
		std::cout << "[MODE] off_sweep=1 -> running sweep\n";
		std::cout << common::ToString(params) << "\n";
		offload::RunOffloadSweep(params);
		std::cout << "[Sweep] DONE\n";
		return 0;
	}
	
	
	const int exp_suite = get_arg_int(argc, argv, "--exp_repair_suite", 0);
	if (exp_suite) {
		const int sessions = get_arg_int(argc, argv, "--exp_trials", 2000);
		const int max_attempts = get_arg_int(argc, argv, "--exp_max_attempts", 2000);
		const int tamper_trials = get_arg_int(argc, argv, "--exp_tamper_trials", 200);
		const int refresh_sessions = get_arg_int(argc, argv, "--exp_refresh_sessions", 2000);
	
		std::cout << "[MODE] exp_repair_suite=1 (A1/A2/B1/B2)\n";
		std::cout << common::ToString(params) << "\n";
	
		run_exp_A1A2_abort_vs_repair(params, sessions, max_attempts);
		run_exp_B1_tamper(params, tamper_trials);
		run_exp_B2_refresh(params, refresh_sessions);
	
		std::cout << "[EXP] suite done.\n";
		return 0;
	}
	
	
	const int exp_a1a2_budget = get_arg_int(argc, argv, "--exp_a1a2_budget", 0);
	if (exp_a1a2_budget) {
		const int sessions = get_arg_int(argc, argv, "--exp_trials", 200);

		std::cout << "[MODE] exp_a1a2_budget=1 (A1/A2 retry budget sweep)\n";
		std::cout << common::ToString(params) << "\n";

		run_exp_A1A2_retry_budget_sweep(params, sessions);

		std::cout << "[EXP] A1A2 budget sweep done.\n";
		return 0;
	}

	
	const int exp_a1a2_boundary = get_arg_int(argc, argv, "--exp_a1a2_boundary", 0);
	if (exp_a1a2_boundary) {
		const int sessions = get_arg_int(argc, argv, "--exp_trials", 200);
		const int max_attempts = get_arg_int(argc, argv, "--exp_max_attempts", 16);

		std::cout << "[MODE] exp_a1a2_boundary=1 (fine boundary sweep)\n";
		std::cout << common::ToString(params) << "\n";

		exp_boundary_sweep::run_exp_A1A2_boundary_sweep(params, sessions, max_attempts);

		std::cout << "[EXP] A1A2 boundary sweep done.\n";
		return 0;
	}

	
	const int exp_c1 = get_arg_int(argc, argv, "--exp_c1", 0);
	if (exp_c1) {
		const int sessions = get_arg_int(argc, argv, "--exp_trials", 2000);
	
		std::cout << "[MODE] exp_c1=1 (prove/verify overhead + proof size)\n";
		std::cout << common::ToString(params) << "\n";
	
		run_exp_C1_overhead_and_size(params, sessions);
	
		std::cout << "[EXP] C1 done.\n";
		return 0;
	}
	
	
	const int exp_d1 = get_arg_int(argc, argv, "--exp_d1", 0);
	if (exp_d1) {
		const int trials = get_arg_int(argc, argv, "--exp_trials", 1000);
	
		std::cout << "[MODE] exp_d1=1 (undetected error vs Freivalds rounds)\n";
		std::cout << common::ToString(params) << "\n";
	
		run_exp_D1_undetected_vs_freivalds(params, trials);
	
		std::cout << "[EXP] D1 done.\n";
		return 0;
	}
	
	
	const int exp_d5 = get_arg_int(argc, argv, "--exp_d5", 0);
	if (exp_d5) {
		const int trials = get_arg_int(argc, argv, "--exp_trials", 1000);
	
		std::cout << "[MODE] exp_d5=1 (delay=0 vs delay>0 minibench)\n";
		std::cout << common::ToString(params) << "\n";
	
		run_exp_D5_delay_compare(params, trials);
	
		std::cout << "[EXP] D5 done.\n";
		return 0;
	}
	
	std::cout << "[INFO] No mode selected.\n";
	print_preset_guide();
	std::cout << "[INFO] Use one of:\n"
		<< "  --selftest_ntt 1\n"
		<< "  --exp_standardized 1\n"
		<< "  --exp_review_supp 1 [--review_seeds 10 --review_sessions 200 --review_trials 3000]\n"
		<< "  --exp_repair_suite 1\n"
		<< "  --exp_a1a2_budget 1\n"
		<< "  --exp_a1a2_boundary 1\n"
		<< "  --exp_c1 1\n"
		<< "  --exp_d1 1\n"
		<< "  --exp_d5 1\n"
		<< "  --off_sweep 1\n";
	std::cout << common::ToString(params) << "\n";
	return 0;
}
