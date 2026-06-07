

#include "offload/sweep.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <unordered_set>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#include <direct.h>
static void ensure_dir(const char* name) { _mkdir(name); }
#else
#include <sys/stat.h>
#include <sys/types.h>
static void ensure_dir(const char* name) { mkdir(name, 0755); }
#endif

#include "common/rng.h"
#include "common/timing.h"
#include "lattice/polymat.h"
#include "lattice/polyvec.h"
#include "lattice/poly.h"
#include "lattice/mod_arith.h"
#include "lattice/ntt.h"

#include "offload/adversary.h"
#include "offload/coded_offload.h"
#include "offload/ntt_offload.h"

namespace offload {

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
	
	static bool file_exists(const std::string& path) {
		std::ifstream ifs(path.c_str(), std::ios::in);
		return (bool)ifs;
	}
	
	static bool copy_file_no_overwrite(const std::string& src, const std::string& dst) {
		if (!file_exists(src)) return false;
		if (file_exists(dst)) return false;
		std::ifstream in(src.c_str(), std::ios::binary);
		std::ofstream out(dst.c_str(), std::ios::binary);
		if (!in || !out) return false;
		out << in.rdbuf();
		out.flush();
		return (bool)out;
	}
	
	static std::vector<std::string> split_csv(const std::string& line) {
		std::vector<std::string> out;
		std::stringstream ss(line);
		std::string item;
		while (std::getline(ss, item, ',')) out.push_back(item);
		return out;
	}
	
	static double percentile(std::vector<double> v, double pct01) {
		if (v.empty()) return 0.0;
		if (pct01 < 0.0) pct01 = 0.0;
		if (pct01 > 1.0) pct01 = 1.0;
		std::sort(v.begin(), v.end());
		const size_t n = v.size();
		const size_t idx = (size_t)(pct01 * (double)(n - 1));
		return v[idx];
	}
	
	static double average(const std::vector<double>& v) {
		if (v.empty()) return 0.0;
		double s = 0.0;
		for (double x : v) s += x;
		return s / (double)v.size();
	}
	
	static int clamp_int(int x, int lo, int hi) {
		if (x < lo) return lo;
		if (x > hi) return hi;
		return x;
	}
	
	static int64_t bytes_per_coeff_est() { return 2; }
	
	static int64_t matvec_bytes_recv_full(const common::Params& p) {
		int K = clamp_int(p.off_k, 1, p.k);
		return (int64_t)K * (int64_t)p.k * (int64_t)p.N * bytes_per_coeff_est();
	}
	
	static int64_t ntt_bytes_recv_full(const common::Params& p) {
		int64_t bytes_vec = (int64_t)p.N * bytes_per_coeff_est();
		int64_t one_ntt = 0;
		for (int len = 1; len < p.N; len <<= 1) {
			int Ks = p.off_k;
			if (Ks < 1) Ks = 1;
			if (Ks > len) Ks = len;
			one_ntt += (int64_t)Ks * bytes_vec;
		}
		return 3 * one_ntt;
	}
	
	static bool poly_equal_modq(const common::Params& p, const lattice::Poly& a, const lattice::Poly& b) {
		for (int i = 0; i < p.N; i++) {
			if (lattice::modq(a.c[i], p.q) != lattice::modq(b.c[i], p.q)) return false;
		}
		return true;
	}
	
	static bool polyvec_equal_modq(const common::Params& p, const lattice::PolyVec& a, const lattice::PolyVec& b) {
		for (int i = 0; i < p.k; i++) {
			for (int j = 0; j < p.N; j++) {
				if (lattice::modq(a.v[i].c[j], p.q) != lattice::modq(b.v[i].c[j], p.q)) return false;
			}
		}
		return true;
	}
	
	static lattice::Poly rand_poly_uniform(const common::Params& p, common::Rng& rng) {
		lattice::Poly a(p.N);
		for (int i = 0; i < p.N; i++) a.c[i] = rng.uniform_int(0, p.q - 1);
		return a;
	}
	
	
	static std::string make_sweep_base_path(const std::string& prefix, const common::Params& base, const char* xof_tag) {
		std::ostringstream oss;
		oss << "data/" << prefix
			<< "_xof" << xof_tag
			<< "_N" << base.N
			<< "_k" << base.k
			<< "_q" << base.q
			<< "_offk" << base.off_k
			<< "_r" << base.off_r
			<< "_thr" << base.off_threaded
			<< "_w" << base.off_workers
			<< "_d" << base.off_delay_us
			<< "_j" << base.off_delay_jitter_us
			<< "_s" << base.off_delay_step_us
			<< "_tr" << base.off_sweep_trials
			<< "_seed" << hex_noprefix_u64(base.seed)
			<< ".csv";
		return oss.str();
	}
	
	static std::string make_sweep_archive_path(const std::string& prefix, const common::Params& base, const char* xof_tag, const std::string& ts) {
		std::ostringstream oss;
		oss << "data/" << prefix
			<< "_" << ts
			<< "_xof" << xof_tag
			<< "_N" << base.N
			<< "_k" << base.k
			<< "_q" << base.q
			<< "_offk" << base.off_k
			<< "_r" << base.off_r
			<< "_thr" << base.off_threaded
			<< "_w" << base.off_workers
			<< "_d" << base.off_delay_us
			<< "_j" << base.off_delay_jitter_us
			<< "_s" << base.off_delay_step_us
			<< "_tr" << base.off_sweep_trials
			<< "_seed" << hex_noprefix_u64(base.seed)
			<< ".csv";
		return oss.str();
	}
	
	
	static std::string make_key_ntt(const common::Params& p, int strag, int mal, int freivalds, int trials) {
		std::stringstream k;
		k << "proto=3|N=" << p.N << "|q=" << p.q << "|offk=" << p.off_k << "|offr=" << p.off_r
			<< "|f=" << freivalds << "|strag=" << strag << "|mal=" << mal
			<< "|thr=" << p.off_threaded << "|w=" << p.off_workers
			<< "|d=" << p.off_delay_us << "|j=" << p.off_delay_jitter_us << "|s=" << p.off_delay_step_us
			<< "|tr=" << trials;
		return k.str();
	}
	
	static std::string make_key_matvec(const common::Params& p, int strag, int mal, int freivalds, int trials) {
		std::stringstream k;
		k << "proto=1|N=" << p.N << "|q=" << p.q << "|k=" << p.k << "|offk=" << p.off_k << "|offr=" << p.off_r
			<< "|f=" << freivalds << "|strag=" << strag << "|mal=" << mal
			<< "|thr=" << p.off_threaded << "|w=" << p.off_workers
			<< "|d=" << p.off_delay_us << "|j=" << p.off_delay_jitter_us << "|s=" << p.off_delay_step_us
			<< "|tr=" << trials;
		return k.str();
	}
	
	static void load_done_keys_ntt(const std::string& path, std::unordered_set<std::string>& done) {
		std::ifstream ifs(path.c_str(), std::ios::in);
		if (!ifs) return;
		std::string line;
		bool first = true;
		while (std::getline(ifs, line)) {
			if (first) { first = false; continue; }
			if (line.empty()) continue;
			auto v = split_csv(line);
			if (v.size() < 15) continue;
			common::Params p;
			p.N = std::atoi(v[1].c_str());
			p.q = std::atoi(v[2].c_str());
			p.k = std::atoi(v[3].c_str());
			p.off_k = std::atoi(v[4].c_str());
			p.off_r = std::atoi(v[5].c_str());
			int f = std::atoi(v[6].c_str());
			int strag = std::atoi(v[7].c_str());
			int mal = std::atoi(v[8].c_str());
			p.off_threaded = std::atoi(v[9].c_str());
			p.off_workers = std::atoi(v[10].c_str());
			p.off_delay_us = std::atoi(v[11].c_str());
			p.off_delay_jitter_us = std::atoi(v[12].c_str());
			p.off_delay_step_us = std::atoi(v[13].c_str());
			int trials = std::atoi(v[14].c_str());
			done.insert(make_key_ntt(p, strag, mal, f, trials));
		}
	}
	
	static void load_done_keys_matvec(const std::string& path, std::unordered_set<std::string>& done) {
		std::ifstream ifs(path.c_str(), std::ios::in);
		if (!ifs) return;
		std::string line;
		bool first = true;
		while (std::getline(ifs, line)) {
			if (first) { first = false; continue; }
			if (line.empty()) continue;
			auto v = split_csv(line);
			if (v.size() < 15) continue;
			common::Params p;
			p.N = std::atoi(v[1].c_str());
			p.q = std::atoi(v[2].c_str());
			p.k = std::atoi(v[3].c_str());
			p.off_k = std::atoi(v[4].c_str());
			p.off_r = std::atoi(v[5].c_str());
			int f = std::atoi(v[6].c_str());
			int strag = std::atoi(v[7].c_str());
			int mal = std::atoi(v[8].c_str());
			p.off_threaded = std::atoi(v[9].c_str());
			p.off_workers = std::atoi(v[10].c_str());
			p.off_delay_us = std::atoi(v[11].c_str());
			p.off_delay_jitter_us = std::atoi(v[12].c_str());
			p.off_delay_step_us = std::atoi(v[13].c_str());
			int trials = std::atoi(v[14].c_str());
			done.insert(make_key_matvec(p, strag, mal, f, trials));
		}
	}
	
	
	struct StageSim {
		int dropped = 0;
		int corrupted = 0;
		int received = 0;
		int waited = 0;
	};
	
	static StageSim simulate_stage(int n, int K, const AdversaryConfig& cfg, common::Rng& rng) {
		StageSim s;
		if (K <= 0) K = 1;
		if (n < K) return s;
	
		std::vector<int> order(n);
		for (int i = 0; i < n; i++) order[i] = i;
		for (int i = n - 1; i > 0; i--) {
			int j = rng.uniform_int(0, i);
			std::swap(order[i], order[j]);
		}
	
		std::vector<int> delivered(n, 1);
		for (int id = 0; id < n; id++) {
			int r = rng.uniform_int(0, 99);
			if (r < cfg.straggler_pct) { delivered[id] = 0; s.dropped++; }
		}
		for (int id = 0; id < n; id++) {
			if (!delivered[id]) continue;
			int r = rng.uniform_int(0, 99);
			if (r < cfg.malicious_pct) s.corrupted++;
		}
	
		int tried = 0;
		int used = 0;
		for (int t = 0; t < n; t++) {
			int id = order[t];
			tried++;
			if (!delivered[id]) continue;
			s.received++;
			used++;
			if (used >= K) break;
		}
		s.waited = (tried > K ? (tried - K) : 0);
		return s;
	}
	
	
	
	
	void RunOffloadSweep(const common::Params& base) {
		if (base.offload_proto == 3) {
			RunNttOffloadSweep(base);
			return;
		}
	
		ensure_dir("data");
		const char* xof_tag = "shake256";
	
		const std::string base_path = make_sweep_base_path("matvec_offload_fullsweep", base, xof_tag);
		const std::string archive_path = make_sweep_archive_path("matvec_offload_fullsweep", base, xof_tag, now_ymdhms());
	
		std::cerr << "[Sweep][MatVec] base_path: " << base_path << "\n";
		std::cerr << "[Sweep][MatVec] archive  : " << archive_path << "\n";
	
		std::unordered_set<std::string> done;
		const bool existed = file_exists(base_path);
		if (existed) load_done_keys_matvec(base_path, done);
	
		std::ofstream ofs(base_path.c_str(), std::ios::out | (existed ? std::ios::app : std::ios::trunc));
		if (!ofs) {
			std::cerr << "[Sweep][MatVec] cannot open " << base_path << "\n";
			return;
		}
	
		if (!existed) {
			ofs << "proto,N,q,k,off_k,off_r,freivalds_rounds,straggler_pct,malicious_pct,"
				<< "threaded,workers,delay_us,jitter_us,step_us,trials,"
				<< "offload_ok_rate,fallback_rate,undetected_error_rate,"
				<< "avg_waited,avg_received,avg_dropped,avg_corrupted,"
				<< "avg_offload_ms,p95_offload_ms,p99_offload_ms,"
				<< "avg_local_ms,p95_local_ms,p99_local_ms,"
				<< "bytes_recv_full,bytes_recv_eff\n";
		}
	
		const int strag_grid[] = { 0, 10, 20, 30, 40, 50 };
		const int mal_grid[] = { 0, 5, 10, 20 };
		const int f_grid[] = { 0, 2, 4, 8 };
	
		common::Rng top_rng(base.seed ^ 0xA5A5A5A55A5A5A5AULL);
	
		for (int strag : strag_grid) {
			for (int mal : mal_grid) {
				for (int rounds : f_grid) {
	
					common::Params p = base;
					p.offload_proto = 1;
					p.off_freivalds = rounds;
					p.off_verbose = 0;
	
					const std::string key = make_key_matvec(p, strag, mal, rounds, p.off_sweep_trials);
					if (done.find(key) != done.end()) {
						std::cerr << "[Sweep][MatVec] SKIP(resume) strag=" << strag
							<< " mal=" << mal << " freivalds=" << rounds << "\n";
						continue;
					}
	
					AdversaryConfig adv;
					adv.straggler_pct = strag;
					adv.malicious_pct = mal;
	
					int trials = (p.off_sweep_trials > 0 ? p.off_sweep_trials : 200);
					int ok_cnt = 0, fail_cnt = 0, und_cnt = 0;
	
					double sum_waited = 0, sum_recv = 0, sum_drop = 0, sum_corr = 0;
	
					std::vector<double> v_off_ms, v_loc_ms;
					v_off_ms.reserve(trials);
					v_loc_ms.reserve(trials);
	
					for (int t = 0; t < trials; t++) {
						uint64_t seed = top_rng.next_u64();
						common::Rng rng(seed);
	
						lattice::PolyMat M = lattice::mat_uniform(p, rng);
						lattice::PolyVec x = lattice::vec_uniform(p, rng);
	
						common::Timer tloc;
						lattice::PolyVec y_local = lattice::mat_vec_mul(p, M, x);
						v_loc_ms.push_back(tloc.ms());
	
						lattice::PolyVec y_off = lattice::vec_zero(p);
						OffloadStats st;
	
						common::Timer toff;
						bool ok = CodedOffloadMatVec(p, M, x, y_off, st, rng, adv);
						v_off_ms.push_back(toff.ms());
	
						sum_waited += st.waited;
						sum_recv += st.received;
						sum_drop += st.dropped;
						sum_corr += st.corrupted;
	
						bool undetected = false;
						if (ok && !polyvec_equal_modq(p, y_off, y_local)) undetected = true;
	
						if (ok) ok_cnt++; else fail_cnt++;
						if (undetected) und_cnt++;
					}
	
					const double ok_rate = (double)ok_cnt / trials;
					const double fb_rate = (double)fail_cnt / trials;
					const double und_rate = (double)und_cnt / trials;
	
					const double avg_waited = sum_waited / trials;
					const double avg_recv = sum_recv / trials;
					const double avg_drop = sum_drop / trials;
					const double avg_corr = sum_corr / trials;
	
					const double avg_off = average(v_off_ms);
					const double p95_off = percentile(v_off_ms, 0.95);
					const double p99_off = percentile(v_off_ms, 0.99);
	
					const double avg_loc = average(v_loc_ms);
					const double p95_loc = percentile(v_loc_ms, 0.95);
					const double p99_loc = percentile(v_loc_ms, 0.99);
	
					const int64_t bytes_full = matvec_bytes_recv_full(p);
					const int64_t bytes_eff = (int64_t)((double)bytes_full * ok_rate);
	
					ofs << 1 << "," << p.N << "," << p.q << "," << p.k << ","
						<< p.off_k << "," << p.off_r << "," << rounds << ","
						<< strag << "," << mal << ","
						<< p.off_threaded << "," << p.off_workers << ","
						<< p.off_delay_us << "," << p.off_delay_jitter_us << "," << p.off_delay_step_us << ","
						<< trials << ","
						<< ok_rate << "," << fb_rate << "," << und_rate << ","
						<< avg_waited << "," << avg_recv << "," << avg_drop << "," << avg_corr << ","
						<< avg_off << "," << p95_off << "," << p99_off << ","
						<< avg_loc << "," << p95_loc << "," << p99_loc << ","
						<< bytes_full << "," << bytes_eff << "\n";
					ofs.flush();
					done.insert(key);
	
					std::cerr << "[Sweep][MatVec] strag=" << strag
						<< " mal=" << mal
						<< " freivalds=" << rounds
						<< " ok=" << ok_rate
						<< " fb=" << fb_rate
						<< " und=" << und_rate
						<< "\n";
				}
			}
		}
	
		ofs.close();
	
		if (copy_file_no_overwrite(base_path, archive_path)) {
			std::cerr << "[Sweep][MatVec] archived(copy) -> " << archive_path << "\n";
		}
		else {
			std::cerr << "[Sweep][MatVec] archive(copy) skipped (exists or error)\n";
		}
	}
	
	
	
	
	void RunNttOffloadSweep(const common::Params& base) {
		ensure_dir("data");
		const char* xof_tag = "shake256";
	
		const std::string base_path = make_sweep_base_path("ntt_offload_fullsweep", base, xof_tag);
		const std::string archive_path = make_sweep_archive_path("ntt_offload_fullsweep", base, xof_tag, now_ymdhms());
	
		std::cerr << "[Sweep][NTT] base_path: " << base_path << "\n";
		std::cerr << "[Sweep][NTT] archive  : " << archive_path << "\n";
	
		std::unordered_set<std::string> done;
		const bool existed = file_exists(base_path);
		if (existed) load_done_keys_ntt(base_path, done);
	
		std::ofstream ofs(base_path.c_str(), std::ios::out | (existed ? std::ios::app : std::ios::trunc));
		if (!ofs) {
			std::cerr << "[Sweep][NTT] cannot open " << base_path << "\n";
			return;
		}
	
		if (!existed) {
			ofs << "proto,N,q,k,off_k,off_r,off_freivalds,straggler_pct,malicious_pct,"
				<< "threaded,workers,delay_us,jitter_us,step_us,trials,"
				<< "offload_ok_rate,fallback_rate,undetected_error_rate,"
				<< "avg_waited,avg_received,avg_dropped,avg_corrupted,"
				<< "avg_offload_ms,p95_offload_ms,p99_offload_ms,"
				<< "avg_local_ms,p95_local_ms,p99_local_ms,"
				<< "bytes_recv_full,bytes_recv_eff\n";
		}
	
		const int strag_grid[] = { 0, 10, 20, 30, 40, 50 };
		const int mal_grid[] = { 0, 5, 10, 20 };
		const int f_grid[] = { 0, 2, 4, 8, 16 };
	
		common::Rng top_rng(base.seed ^ 0xC3C3C3C33C3C3C3CULL);
	
		common::Params pref = base;
		pref.use_ntt = 1;
		pref.offload_proto = 0;
		pref.off_verbose = 0;
	
		for (int strag : strag_grid) {
			for (int mal : mal_grid) {
				for (int rounds : f_grid) {
	
					common::Params p = base;
					p.use_ntt = 1;
					p.offload_proto = 3;
					p.off_freivalds = rounds;
					p.off_straggler_pct = strag;
					p.off_malicious_pct = mal;
					p.off_verbose = 0;
	
					const std::string key = make_key_ntt(p, strag, mal, rounds, p.off_sweep_trials);
					if (done.find(key) != done.end()) {
						std::cerr << "[Sweep][NTT] SKIP(resume) strag=" << strag
							<< " mal=" << mal << " off_freivalds=" << rounds << "\n";
						continue;
					}
	
					AdversaryConfig adv;
					adv.straggler_pct = strag;
					adv.malicious_pct = mal;
	
					int trials = (p.off_sweep_trials > 0 ? p.off_sweep_trials : 200);
					int ok_cnt = 0, fail_cnt = 0, und_cnt = 0;
	
					std::vector<double> v_off_ms, v_loc_ms;
					v_off_ms.reserve(trials);
					v_loc_ms.reserve(trials);
	
					double waited_sum = 0, recv_sum = 0, drop_sum = 0, corr_sum = 0;
					int stage_cnt_total = 0;
	
					for (int t = 0; t < trials; t++) {
						uint64_t seed = top_rng.next_u64();
						common::Rng rng(seed);
	
						lattice::Poly a = rand_poly_uniform(p, rng);
						lattice::Poly b = rand_poly_uniform(p, rng);
	
						common::Timer tloc;
						lattice::Poly c_ref = lattice::ntt::mul_ntt(pref, a, b);
						v_loc_ms.push_back(tloc.ms());
	
						lattice::Poly c_off(p.N);
						NttOffloadStats st;
	
						common::Timer toff;
						bool ok = offload::PolyMulOffload(p, a, b, c_off, rng, adv, &st);
						v_off_ms.push_back(toff.ms());
	
						bool undetected = false;
						if (ok && !poly_equal_modq(p, c_off, c_ref)) undetected = true;
	
						if (ok) ok_cnt++; else fail_cnt++;
						if (undetected) und_cnt++;
	
						common::Rng sim_rng(seed ^ 0x9E3779B97F4A7C15ULL);
						for (int rep = 0; rep < 3; rep++) {
							for (int len = 1; len < p.N; len <<= 1) {
								int K = p.off_k;
								if (K < 1) K = 1;
								if (K > len) K = len;
								int r = (p.off_r < 0 ? 0 : p.off_r);
								int n = K + r;
								StageSim s = simulate_stage(n, K, adv, sim_rng);
								waited_sum += (double)s.waited;
								recv_sum += (double)s.received;
								drop_sum += (double)s.dropped;
								corr_sum += (double)s.corrupted;
								stage_cnt_total++;
							}
						}
					}
	
					const double ok_rate = (double)ok_cnt / trials;
					const double fb_rate = (double)fail_cnt / trials;
					const double und_rate = (double)und_cnt / trials;
	
					const double avg_waited = (stage_cnt_total ? waited_sum / stage_cnt_total : 0.0);
					const double avg_recv = (stage_cnt_total ? recv_sum / stage_cnt_total : 0.0);
					const double avg_drop = (stage_cnt_total ? drop_sum / stage_cnt_total : 0.0);
					const double avg_corr = (stage_cnt_total ? corr_sum / stage_cnt_total : 0.0);
	
					const double avg_off = average(v_off_ms);
					const double p95_off = percentile(v_off_ms, 0.95);
					const double p99_off = percentile(v_off_ms, 0.99);
	
					const double avg_loc = average(v_loc_ms);
					const double p95_loc = percentile(v_loc_ms, 0.95);
					const double p99_loc = percentile(v_loc_ms, 0.99);
	
					const int64_t bytes_full = ntt_bytes_recv_full(p);
					const int64_t bytes_eff = (int64_t)((double)bytes_full * ok_rate);
	
					ofs << 3 << "," << p.N << "," << p.q << "," << p.k << ","
						<< p.off_k << "," << p.off_r << "," << rounds << ","
						<< strag << "," << mal << ","
						<< p.off_threaded << "," << p.off_workers << ","
						<< p.off_delay_us << "," << p.off_delay_jitter_us << "," << p.off_delay_step_us << ","
						<< trials << ","
						<< ok_rate << "," << fb_rate << "," << und_rate << ","
						<< avg_waited << "," << avg_recv << "," << avg_drop << "," << avg_corr << ","
						<< avg_off << "," << p95_off << "," << p99_off << ","
						<< avg_loc << "," << p95_loc << "," << p99_loc << ","
						<< bytes_full << "," << bytes_eff << "\n";
					ofs.flush();
					done.insert(key);
	
					std::cerr << "[Sweep][NTT] strag=" << strag
						<< " mal=" << mal
						<< " off_freivalds=" << rounds
						<< " ok=" << ok_rate
						<< " fb=" << fb_rate
						<< " und=" << und_rate
						<< " off_ms(p95)=" << p95_off
						<< "\n";
				}
			}
		}
	
		ofs.close();
	
		if (copy_file_no_overwrite(base_path, archive_path)) {
			std::cerr << "[Sweep][NTT] archived(copy) -> " << archive_path << "\n";
		}
		else {
			std::cerr << "[Sweep][NTT] archive(copy) skipped (exists or error)\n";
		}
	}

} 
