#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
#include <direct.h>
static void ensure_dir_d1(const char* name) { _mkdir(name); }
#else
#include <sys/stat.h>
#include <sys/types.h>
static void ensure_dir_d1(const char* name) { mkdir(name, 0755); }
#endif

#include "common/params.h"
#include "common/rng.h"
#include "common/timing.h"

#include "lattice/polymat.h"
#include "lattice/polyvec.h"
#include "lattice/mod_arith.h"

#include "offload/adversary.h"
#include "offload/coded_offload.h"

namespace {

	static std::string now_ymdhms_d1() {
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

	static std::string hex_noprefix_u64_d1(uint64_t x) {
		std::ostringstream oss;
		oss << std::hex << std::nouppercase << x;
		return oss.str();
	}
	
	static bool poly_equal_modq_d1(const common::Params& p,
		const lattice::Poly& a,
		const lattice::Poly& b) {
		if (a.c.size() != b.c.size()) return false;
		for (size_t i = 0; i < a.c.size(); i++) {
			if (lattice::modq(a.c[i], p.q) != lattice::modq(b.c[i], p.q)) return false;
		}
		return true;
	}
	
	static bool polyvec_equal_modq_d1(const common::Params& p,
		const lattice::PolyVec& a,
		const lattice::PolyVec& b) {
		if (a.v.size() != b.v.size()) return false;
		for (size_t i = 0; i < a.v.size(); i++) {
			if (!poly_equal_modq_d1(p, a.v[i], b.v[i])) return false;
		}
		return true;
	}
	
	static double mean_of(const std::vector<double>& v) {
		if (v.empty()) return 0.0;
		double s = 0.0;
		for (size_t i = 0; i < v.size(); i++) s += v[i];
		return s / (double)v.size();
	}
	
	static double quantile_of(std::vector<double> v, double q) {
		if (v.empty()) return 0.0;
		if (q <= 0.0) q = 0.0;
		if (q >= 1.0) q = 1.0;
		std::sort(v.begin(), v.end());
		size_t idx = (size_t)std::floor(q * (double)(v.size() - 1));
		return v[idx];
	}
	
	static double theory_undetected_bound(int rounds) {
		if (rounds <= 0) return 1.0;
		return std::ldexp(1.0, -rounds); 
	}
	
	static size_t bytes_poly(const common::Params& p) {
		return (size_t)p.N * sizeof(int16_t);
	}
	
	static size_t bytes_polyvec(const common::Params& p) {
		return (size_t)p.k * bytes_poly(p);
	}
	
	static size_t bytes_matvec_result_full(const common::Params& p) {
		
		return bytes_polyvec(p);
	}

} 

void run_exp_D1_undetected_vs_freivalds(const common::Params& base, int trials) {
	ensure_dir_d1("data");

	const std::string ts = now_ymdhms_d1();
	const char* xof = "shake256";
	
	
	const int straggler_fixed = 30;
	const int mal_grid[] = { 0, 5, 10, 20 };
	const int f_grid[] = { 0, 1, 2, 4, 8, 16 };
	
	std::ostringstream fn;
	fn << "data/exp_D1_undetected_vs_freivalds_" << ts
		<< "_xof" << xof
		<< "_N" << base.N << "_k" << base.k << "_q" << base.q
		<< "_trials" << trials
		<< "_seed" << hex_noprefix_u64_d1(base.seed)
		<< ".csv";
	
	std::ofstream ofs(fn.str().c_str(), std::ios::out | std::ios::trunc);
	if (!ofs) {
		std::cout << "cannot open " << fn.str() << "\n";
		return;
	}
	
	ofs << "straggler_pct,malicious_pct,freivalds_rounds,trials,"
		"ok_cnt,fail_cnt,undetected_cnt,"
		"offload_ok_rate,fallback_rate,undetected_error_rate,undetected_given_ok_rate,"
		"theory_2pow_minus_f,"
		"avg_waited,avg_received,avg_dropped,avg_corrupted,"
		"avg_offload_ms,p95_offload_ms,p99_offload_ms,"
		"avg_local_ms,p95_local_ms,p99_local_ms,"
		"bytes_recv_full,bytes_recv_eff\n";
	
	common::Rng top_rng(base.seed ^ 0xD100D100A55AA55AULL);
	
	for (size_t mi = 0; mi < sizeof(mal_grid) / sizeof(mal_grid[0]); mi++) {
		for (size_t fi = 0; fi < sizeof(f_grid) / sizeof(f_grid[0]); fi++) {
			common::Params p = base;
			p.offload_proto = 1;                 
			p.off_freivalds = f_grid[fi];
			p.off_verbose = 0;
	
			offload::AdversaryConfig adv;
			adv.straggler_pct = straggler_fixed;
			adv.malicious_pct = mal_grid[mi];
	
			int ok_cnt = 0;
			int fail_cnt = 0;
			int und_cnt = 0;
	
			double sum_waited = 0.0;
			double sum_recv = 0.0;
			double sum_drop = 0.0;
			double sum_corr = 0.0;
	
			std::vector<double> v_off_ms;
			std::vector<double> v_loc_ms;
			v_off_ms.reserve((size_t)trials);
			v_loc_ms.reserve((size_t)trials);
	
			size_t bytes_recv_full = bytes_matvec_result_full(p);
			double sum_bytes_recv_eff = 0.0;
	
			for (int t = 0; t < trials; t++) {
				uint64_t seed = top_rng.next_u64();
				common::Rng rng(seed);
	
				lattice::PolyMat M = lattice::mat_uniform(p, rng);
				lattice::PolyVec x = lattice::vec_uniform(p, rng);
	
				common::Timer tloc;
				lattice::PolyVec y_local = lattice::mat_vec_mul(p, M, x);
				v_loc_ms.push_back(tloc.ms());
	
				lattice::PolyVec y_off = lattice::vec_zero(p);
				offload::OffloadStats st;
	
				common::Timer toff;
				bool ok = offload::CodedOffloadMatVec(p, M, x, y_off, st, rng, adv);
				v_off_ms.push_back(toff.ms());
	
				sum_waited += (double)st.waited;
				sum_recv += (double)st.received;
				sum_drop += (double)st.dropped;
				sum_corr += (double)st.corrupted;
	
				bool undetected = false;
				if (ok && !polyvec_equal_modq_d1(p, y_off, y_local)) {
					undetected = true;
				}
	
				if (ok) ok_cnt++;
				else fail_cnt++;
				if (undetected) und_cnt++;
	
				
				sum_bytes_recv_eff += (double)st.received * (double)bytes_recv_full;
			}
	
			const double offload_ok_rate = (trials > 0 ? (double)ok_cnt / (double)trials : 0.0);
			const double fallback_rate = (trials > 0 ? (double)fail_cnt / (double)trials : 0.0);
			const double undetected_error_rate = (trials > 0 ? (double)und_cnt / (double)trials : 0.0);
			const double undetected_given_ok_rate = (ok_cnt > 0 ? (double)und_cnt / (double)ok_cnt : 0.0);
	
			ofs << straggler_fixed << ","
				<< mal_grid[mi] << ","
				<< f_grid[fi] << ","
				<< trials << ","
				<< ok_cnt << ","
				<< fail_cnt << ","
				<< und_cnt << ","
				<< offload_ok_rate << ","
				<< fallback_rate << ","
				<< undetected_error_rate << ","
				<< undetected_given_ok_rate << ","
				<< theory_undetected_bound(f_grid[fi]) << ","
				<< (trials > 0 ? sum_waited / (double)trials : 0.0) << ","
				<< (trials > 0 ? sum_recv / (double)trials : 0.0) << ","
				<< (trials > 0 ? sum_drop / (double)trials : 0.0) << ","
				<< (trials > 0 ? sum_corr / (double)trials : 0.0) << ","
				<< mean_of(v_off_ms) << ","
				<< quantile_of(v_off_ms, 0.95) << ","
				<< quantile_of(v_off_ms, 0.99) << ","
				<< mean_of(v_loc_ms) << ","
				<< quantile_of(v_loc_ms, 0.95) << ","
				<< quantile_of(v_loc_ms, 0.99) << ","
				<< (uint64_t)bytes_recv_full << ","
				<< (trials > 0 ? sum_bytes_recv_eff / (double)trials : 0.0)
				<< "\n";
		}
	}
	
	ofs.flush();
	std::cout << "[EXP] wrote: " << fn.str() << "\n";
}
