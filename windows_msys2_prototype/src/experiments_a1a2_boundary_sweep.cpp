#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
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
#include "common/rng.h"
#include "common/timing.h"
#include "common/util.h"

#include "lattice/module_lwe.h"
#include "lattice/mod_arith.h"
#include "preauth/preauth.h"

static std::string now_ymdhms_boundary() {
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

static std::string hex_noprefix_u64_boundary(uint64_t x) {
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << x;
    return oss.str();
}

namespace exp_boundary_sweep {

struct BoundaryCase {
    int B;
    int eta;
};

static const uint64_t kScope = 0x1111ULL;

static std::vector<BoundaryCase> make_cases() {
    
    
    std::vector<BoundaryCase> cases;
    for (int B = 3; B <= 8; ++B) {
        int eta = 11 - B; 
        if (eta < 2) eta = 2;
        if (eta > 6) eta = 6;
        cases.push_back({B, eta});
    }
    return cases;
}

static bool run_one_repair_session(const common::Params& p,
                                   const lattice::KeyPair& kp,
                                   common::Rng& rng,
                                   int& attempts_out,
                                   double& total_ms_out,
                                   int& zstar_maxabs_out,
                                   int& clamp_count_out) {
    preauth::Prover P(p, kp.pk, kp.sk);
    preauth::Verifier V(p, kp.pk);
    P.set_scope(kScope);
    V.set_scope(kScope);

    common::Timer tt;
    preauth::ProveObs obs;
    preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);
    bool ok = V.verify_nizk(pf);
    if (ok) P.on_accept(pf.cm, pf.rsp);

    attempts_out = 1;
    total_ms_out = tt.ms();
    zstar_maxabs_out = obs.zstar_maxabs_centered;
    clamp_count_out = obs.clamp_count;
    return ok;
}

static bool run_one_abort_session(const common::Params& p,
                                  const lattice::KeyPair& kp,
                                  common::Rng& rng,
                                  int max_attempts,
                                  int& attempts_out,
                                  double& total_ms_out,
                                  int& zstar_maxabs_last_out,
                                  int& clamp_count_last_out) {
    preauth::Prover P(p, kp.pk, kp.sk);
    preauth::Verifier V(p, kp.pk);
    P.set_scope(kScope);
    V.set_scope(kScope);

    common::Timer tt;
    bool ok = false;
    int attempts = 0;

    zstar_maxabs_last_out = 0;
    clamp_count_last_out = 0;

    for (attempts = 1; attempts <= max_attempts; ++attempts) {
        preauth::ProveObs obs;
        preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);

        zstar_maxabs_last_out = obs.zstar_maxabs_centered;
        clamp_count_last_out = obs.clamp_count;

        
        
        if (obs.zstar_maxabs_centered > p.B) {
            continue;
        }

        ok = V.verify_nizk(pf);
        if (ok) {
            P.on_accept(pf.cm, pf.rsp);
            break;
        }
    }

    attempts_out = attempts;
    total_ms_out = tt.ms();
    return ok;
}

void run_exp_A1A2_boundary_sweep(const common::Params& base, int sessions, int max_attempts) {
    ensure_dir("data");
    const std::string ts = now_ymdhms_boundary();
    const char* xof = "shake256";

    std::ostringstream raw_fn, sum_fn;
    raw_fn << "data/exp_A1A2_boundary_sweep_raw_" << ts
           << "_xof" << xof
           << "_N" << base.N << "_k" << base.k << "_q" << base.q
           << "_sessions" << sessions
           << "_seed" << hex_noprefix_u64_boundary(base.seed)
           << ".csv";

    sum_fn << "data/exp_A1A2_boundary_sweep_summary_" << ts
           << "_xof" << xof
           << "_N" << base.N << "_k" << base.k << "_q" << base.q
           << "_sessions" << sessions
           << "_seed" << hex_noprefix_u64_boundary(base.seed)
           << ".csv";

    std::ofstream raw_ofs(raw_fn.str().c_str(), std::ios::out | std::ios::trunc);
    std::ofstream sum_ofs(sum_fn.str().c_str(), std::ios::out | std::ios::trunc);
    if (!raw_ofs || !sum_ofs) {
        std::cout << "cannot open boundary sweep outputs\n";
        return;
    }

    raw_ofs << "case_idx,B,eta,mode,budget,session_idx,attempts,ok,total_ms,zstar_maxabs,clamp_count\n";
    sum_ofs << "case_idx,B,eta,mode,budget,sessions,success_rate,avg_attempts,avg_total_ms,avg_zstar_maxabs,avg_clamp_count\n";
    raw_ofs.flush();
    sum_ofs.flush();

    std::vector<BoundaryCase> cases = make_cases();

    for (int ci = 0; ci < (int)cases.size(); ++ci) {
        common::Params p = base;
        p.B = cases[ci].B;
        p.eta = cases[ci].eta;
        p.offload_proto = 0;
        p.off_delay_us = 0;
        p.off_delay_jitter_us = 0;
        p.off_delay_step_us = 0;

        common::Rng krng(p.seed ^ (uint64_t)ci * 0x9E3779B97F4A7C15ULL);
        lattice::KeyPair kp = lattice::keygen(p, krng);

        
        {
            int ok_cnt = 0;
            long long attempts_sum = 0;
            double total_sum = 0.0;
            double zstar_sum = 0.0;
            double clamp_sum = 0.0;
            common::Rng rng(p.seed ^ 0xA1A20001ULL ^ (uint64_t)ci);

            for (int si = 0; si < sessions; ++si) {
                int attempts = 0;
                double total_ms = 0.0;
                int zstar_maxabs = 0;
                int clamp_count = 0;
                bool ok = run_one_repair_session(p, kp, rng, attempts, total_ms, zstar_maxabs, clamp_count);
                raw_ofs << ci << "," << p.B << "," << p.eta << ",repair,-1,"
                        << si << "," << attempts << "," << (ok ? 1 : 0) << ","
                        << total_ms << "," << zstar_maxabs << "," << clamp_count << "\n";
                if (si % 10 == 0) raw_ofs.flush();
                if (ok) ok_cnt++;
                attempts_sum += attempts;
                total_sum += total_ms;
                zstar_sum += (double)zstar_maxabs;
                clamp_sum += (double)clamp_count;
            }

            sum_ofs << ci << "," << p.B << "," << p.eta << ",repair,-1,"
                    << sessions << ","
                    << ((double)ok_cnt / sessions) << ","
                    << ((double)attempts_sum / sessions) << ","
                    << (total_sum / sessions) << ","
                    << (zstar_sum / sessions) << ","
                    << (clamp_sum / sessions) << "\n";
            sum_ofs.flush();
        }

        
        {
            int ok_cnt = 0;
            long long attempts_sum = 0;
            double total_sum = 0.0;
            double zstar_sum = 0.0;
            double clamp_sum = 0.0;
            common::Rng rng(p.seed ^ 0xA1A20002ULL ^ (uint64_t)ci);

            for (int si = 0; si < sessions; ++si) {
                int attempts = 0;
                double total_ms = 0.0;
                int zstar_maxabs_last = 0;
                int clamp_count_last = 0;
                bool ok = run_one_abort_session(p, kp, rng, max_attempts, attempts, total_ms,
                                                zstar_maxabs_last, clamp_count_last);
                raw_ofs << ci << "," << p.B << "," << p.eta << ",abort_baseline,"
                        << max_attempts << ","
                        << si << "," << attempts << "," << (ok ? 1 : 0) << ","
                        << total_ms << "," << zstar_maxabs_last << "," << clamp_count_last << "\n";
                if (si % 10 == 0) raw_ofs.flush();
                if (ok) ok_cnt++;
                attempts_sum += attempts;
                total_sum += total_ms;
                zstar_sum += (double)zstar_maxabs_last;
                clamp_sum += (double)clamp_count_last;
            }

            sum_ofs << ci << "," << p.B << "," << p.eta << ",abort_baseline,"
                    << max_attempts << ","
                    << sessions << ","
                    << ((double)ok_cnt / sessions) << ","
                    << ((double)attempts_sum / sessions) << ","
                    << (total_sum / sessions) << ","
                    << (zstar_sum / sessions) << ","
                    << (clamp_sum / sessions) << "\n";
            sum_ofs.flush();
        }

        std::cout << "[EXP][A1A2-boundary] finished case B=" << p.B
                  << " eta=" << p.eta
                  << " budget=" << max_attempts << "\n";
    }

    raw_ofs.flush();
    sum_ofs.flush();
    std::cout << "[EXP] wrote: " << raw_fn.str() << "\n";
    std::cout << "[EXP] wrote: " << sum_fn.str() << "\n";
}

} 
