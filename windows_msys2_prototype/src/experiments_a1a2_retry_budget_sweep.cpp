#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <ctime>
#include <cstdio>

#ifdef _WIN32
#include <direct.h>
static void ensure_dir_a1a2rb(const char* name) { _mkdir(name); }
#else
#include <sys/stat.h>
#include <sys/types.h>
static void ensure_dir_a1a2rb(const char* name) { mkdir(name, 0755); }
#endif

#include "common/params.h"
#include "common/rng.h"
#include "common/timing.h"

#include "lattice/module_lwe.h"
#include "preauth/preauth.h"

namespace {

struct CaseRB {
    int B;
    int eta;
    const char* label;
};

struct AggRB {
    int count = 0;
    int ok_sum = 0;
    double attempts_sum = 0.0;
    double total_ms_sum = 0.0;
    double prove_ms_sum = 0.0;
    double verify_ms_sum = 0.0;
    double zstar_sum = 0.0;
    double clamp_sum = 0.0;
    double audit_sum = 0.0;
};

static std::string now_ymdhms_a1a2rb() {
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

static std::string hex_noprefix_u64_a1a2rb(uint64_t x) {
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << x;
    return oss.str();
}

static void flush_outputs(std::ofstream& raw_ofs, std::ofstream& sum_ofs) {
    raw_ofs.flush();
    sum_ofs.flush();
}

static void write_summary_row(std::ofstream& ofs,
    int case_id,
    const CaseRB& cs,
    const char* mode,
    int budget,
    const AggRB& a) {

    const double denom = (a.count > 0 ? (double)a.count : 1.0);
    ofs << case_id << ","
        << cs.label << ","
        << cs.B << ","
        << cs.eta << ","
        << mode << ","
        << budget << ","
        << a.count << ","
        << (a.ok_sum / denom) << ","
        << (a.attempts_sum / denom) << ","
        << (a.total_ms_sum / denom) << ","
        << (a.prove_ms_sum / denom) << ","
        << (a.verify_ms_sum / denom) << ","
        << (a.zstar_sum / denom) << ","
        << (a.clamp_sum / denom) << ","
        << (a.audit_sum / denom) << "\n";
}

static void run_repair_case_a1a2rb(const common::Params& p,
    const lattice::KeyPair& kp,
    int case_id,
    const CaseRB& cs,
    int sessions,
    std::ofstream& raw_ofs,
    AggRB& agg) {

    const uint64_t scope = 0x1111ULL;

    common::Rng rng(p.seed ^ 0xA1A2B1B2ULL ^ (uint64_t)case_id);
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

        raw_ofs << case_id << ","
                << cs.label << ","
                << p.B << ","
                << p.eta << ",repair,1,"
                << si << ",1,"
                << (ok ? 1 : 0) << ","
                << total_ms << ","
                << prove_ms << ","
                << verify_ms << ","
                << obs.zstar_maxabs_centered << ","
                << obs.clamp_count << ","
                << (uint64_t)obs.audit_cnt << ","
                << obs.c_scalar << "\n";

        if (((si + 1) % 10) == 0 || si + 1 == sessions) {
            raw_ofs.flush();
        }

        agg.count += 1;
        agg.ok_sum += (ok ? 1 : 0);
        agg.attempts_sum += 1.0;
        agg.total_ms_sum += total_ms;
        agg.prove_ms_sum += prove_ms;
        agg.verify_ms_sum += verify_ms;
        agg.zstar_sum += (double)obs.zstar_maxabs_centered;
        agg.clamp_sum += (double)obs.clamp_count;
        agg.audit_sum += (double)obs.audit_cnt;
    }
}

static void run_abort_case_budget_a1a2rb(const common::Params& p,
    const lattice::KeyPair& kp,
    int case_id,
    const CaseRB& cs,
    int sessions,
    int budget,
    std::ofstream& raw_ofs,
    AggRB& agg) {

    const uint64_t scope = 0x1111ULL;

    common::Rng rng(p.seed ^ 0xBADC0FFEEULL ^ (uint64_t)case_id ^ ((uint64_t)budget << 32));
    preauth::Prover P(p, kp.pk, kp.sk);
    preauth::Verifier V(p, kp.pk);
    P.set_scope(scope);
    V.set_scope(scope);

    for (int si = 0; si < sessions; si++) {
        int attempts = 0;
        bool ok = false;
        double prove_sum = 0.0;
        double verify_ms = 0.0;
        preauth::ProveObs obs_last;

        common::Timer t_session;

        while (attempts < budget) {
            attempts++;

            preauth::ProveObs obs;
            common::Timer tp;
            preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);
            double prove_ms = tp.ms();
            prove_sum += prove_ms;

            
            if (obs.zstar_maxabs_centered > p.B) {
                obs_last = obs;
                continue;
            }

            common::Timer tv;
            ok = V.verify_nizk(pf);
            verify_ms = tv.ms();
            obs_last = obs;

            if (ok) P.on_accept(pf.cm, pf.rsp);
            break;
        }

        const double total_ms = t_session.ms();

        raw_ofs << case_id << ","
                << cs.label << ","
                << p.B << ","
                << p.eta << ",abort_baseline,"
                << budget << ","
                << si << ","
                << attempts << ","
                << (ok ? 1 : 0) << ","
                << total_ms << ","
                << prove_sum << ","
                << verify_ms << ","
                << obs_last.zstar_maxabs_centered << ","
                << obs_last.clamp_count << ","
                << (uint64_t)obs_last.audit_cnt << ","
                << obs_last.c_scalar << "\n";

        if (((si + 1) % 10) == 0 || si + 1 == sessions) {
            raw_ofs.flush();
        }

        agg.count += 1;
        agg.ok_sum += (ok ? 1 : 0);
        agg.attempts_sum += (double)attempts;
        agg.total_ms_sum += total_ms;
        agg.prove_ms_sum += prove_sum;
        agg.verify_ms_sum += verify_ms;
        agg.zstar_sum += (double)obs_last.zstar_maxabs_centered;
        agg.clamp_sum += (double)obs_last.clamp_count;
        agg.audit_sum += (double)obs_last.audit_cnt;
    }
}

} 

void run_exp_A1A2_retry_budget_sweep(const common::Params& base, int sessions) {
    ensure_dir_a1a2rb("data");
    const std::string ts = now_ymdhms_a1a2rb();
    const char* xof = "shake256";

    std::vector<CaseRB> cases;
    cases.push_back(CaseRB{8, 2, "loose"});
    cases.push_back(CaseRB{4, 4, "medium"});
    cases.push_back(CaseRB{3, 6, "tight"});

    std::vector<int> budgets;
    budgets.push_back(1);
    budgets.push_back(2);
    budgets.push_back(4);
    budgets.push_back(8);
    budgets.push_back(16);
    budgets.push_back(32);
    budgets.push_back(64);
    budgets.push_back(128);
    budgets.push_back(256);
    budgets.push_back(512);
    budgets.push_back(1024);
    budgets.push_back(2000);

    std::ostringstream raw_fn;
    raw_fn << "data/exp_A1A2_retry_budget_sweep_raw_" << ts
           << "_xof" << xof
           << "_N" << base.N << "_k" << base.k << "_q" << base.q
           << "_sessions" << sessions
           << "_seed" << hex_noprefix_u64_a1a2rb(base.seed)
           << ".csv";

    std::ostringstream sum_fn;
    sum_fn << "data/exp_A1A2_retry_budget_sweep_summary_" << ts
           << "_xof" << xof
           << "_N" << base.N << "_k" << base.k << "_q" << base.q
           << "_sessions" << sessions
           << "_seed" << hex_noprefix_u64_a1a2rb(base.seed)
           << ".csv";

    std::ofstream raw_ofs(raw_fn.str().c_str(), std::ios::out | std::ios::trunc);
    if (!raw_ofs) {
        std::cout << "cannot open " << raw_fn.str() << "\n";
        return;
    }
    std::ofstream sum_ofs(sum_fn.str().c_str(), std::ios::out | std::ios::trunc);
    if (!sum_ofs) {
        std::cout << "cannot open " << sum_fn.str() << "\n";
        return;
    }

    raw_ofs << "case_id,case_label,B,eta,mode,budget,session_idx,attempts,ok,total_ms,prove_ms_sum,verify_ms,"
               "zstar_maxabs_centered,clamp_count,audit_cnt,c_scalar\n";

    sum_ofs << "case_id,case_label,B,eta,mode,budget,sessions,success_rate,avg_attempts,avg_total_ms,avg_prove_ms_sum,avg_verify_ms,"
               "avg_zstar_maxabs_centered,avg_clamp_count,avg_audit_cnt\n";

    
    flush_outputs(raw_ofs, sum_ofs);

    for (int ci = 0; ci < (int)cases.size(); ci++) {
        common::Params p = base;
        p.offload_proto = 0;
        p.off_delay_us = 0;
        p.off_delay_jitter_us = 0;
        p.off_delay_step_us = 0;
        p.B = cases[(size_t)ci].B;
        p.eta = cases[(size_t)ci].eta;

        common::Rng krng(p.seed ^ (uint64_t)ci * 0x9E3779B97F4A7C15ULL);
        lattice::KeyPair kp = lattice::keygen(p, krng);

        std::cout << "[EXP][A1A2-budget] case=" << cases[(size_t)ci].label
                  << " B=" << p.B << " eta=" << p.eta << " -> repair\n";

        AggRB repair_agg;
        run_repair_case_a1a2rb(p, kp, ci, cases[(size_t)ci], sessions, raw_ofs, repair_agg);
        write_summary_row(sum_ofs, ci, cases[(size_t)ci], "repair", 1, repair_agg);
        flush_outputs(raw_ofs, sum_ofs);

        std::cout << "[EXP][A1A2-budget] done case=" << cases[(size_t)ci].label
                  << " mode=repair budget=1 success_rate="
                  << (repair_agg.count > 0 ? ((double)repair_agg.ok_sum / (double)repair_agg.count) : 0.0)
                  << " avg_attempts="
                  << (repair_agg.count > 0 ? (repair_agg.attempts_sum / (double)repair_agg.count) : 0.0)
                  << "\n";

        for (size_t bi = 0; bi < budgets.size(); bi++) {
            const int budget = budgets[bi];

            std::cout << "[EXP][A1A2-budget] case=" << cases[(size_t)ci].label
                      << " B=" << p.B << " eta=" << p.eta
                      << " -> abort budget=" << budget << "\n";

            AggRB abort_agg;
            run_abort_case_budget_a1a2rb(p, kp, ci, cases[(size_t)ci], sessions, budget, raw_ofs, abort_agg);
            write_summary_row(sum_ofs, ci, cases[(size_t)ci], "abort_baseline", budget, abort_agg);
            flush_outputs(raw_ofs, sum_ofs);

            std::cout << "[EXP][A1A2-budget] done case=" << cases[(size_t)ci].label
                      << " mode=abort budget=" << budget
                      << " success_rate="
                      << (abort_agg.count > 0 ? ((double)abort_agg.ok_sum / (double)abort_agg.count) : 0.0)
                      << " avg_attempts="
                      << (abort_agg.count > 0 ? (abort_agg.attempts_sum / (double)abort_agg.count) : 0.0)
                      << "\n";
        }
    }

    flush_outputs(raw_ofs, sum_ofs);

    std::cout << "[EXP] wrote raw    : " << raw_fn.str() << "\n";
    std::cout << "[EXP] wrote summary: " << sum_fn.str() << "\n";
}
