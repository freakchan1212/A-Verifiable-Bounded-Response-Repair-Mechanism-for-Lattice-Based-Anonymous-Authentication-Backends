#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
static void ensure_dir_review(const char* name) { _mkdir(name); }
#else
#include <sys/stat.h>
#include <sys/types.h>
static void ensure_dir_review(const char* name) { mkdir(name, 0755); }
#endif

#include "common/params.h"
#include "common/rng.h"
#include "common/timing.h"
#include "lattice/module_lwe.h"
#include "lattice/mod_arith.h"
#include "preauth/preauth.h"
#include "preauth/statement.h"
#include "common/bytes.h"

namespace review_supp {
namespace {

struct BoundaryCase {
    int B;
    int eta;
    const char* label;
};

struct SeedAgg {
    double success_rate = 0.0;
    double avg_attempts = 0.0;
    double avg_total_ms = 0.0;
    double median_ms = 0.0;
    double p95_ms = 0.0;
    double p99_ms = 0.0;
    double std_ms = 0.0;
    double ci95_ms = 0.0;
};

struct SummaryAgg {
    int seeds = 0;
    int sessions_per_seed = 0;
    std::vector<double> success_rates;
    std::vector<double> attempts;
    std::vector<double> totals;
    std::vector<double> medians;
    std::vector<double> p95s;
    std::vector<double> p99s;
};

struct KeyMS {
    std::string experiment;
    std::string profile;
    std::string boundary;
    std::string method;
    int budget;
    bool operator<(const KeyMS& o) const {
        if (experiment != o.experiment) return experiment < o.experiment;
        if (profile != o.profile) return profile < o.profile;
        if (boundary != o.boundary) return boundary < o.boundary;
        if (method != o.method) return method < o.method;
        return budget < o.budget;
    }
};

static std::string profile_name(const common::Params& p) {
    std::ostringstream oss;
    oss << "preset" << p.preset << "_N" << p.N << "_q" << p.q << "_k" << p.k;
    return oss.str();
}

static std::string out_path(const common::Params& p, const std::string& stem) {
    std::ostringstream oss;
    oss << "data/review_supp/" << profile_name(p) << "_" << stem << ".csv";
    return oss.str();
}

static std::vector<BoundaryCase> default_cases() {
    std::vector<BoundaryCase> v;
    v.push_back(BoundaryCase{8, 2, "loose"});
    v.push_back(BoundaryCase{4, 4, "medium"});
    v.push_back(BoundaryCase{3, 6, "tight"});
    return v;
}

static std::vector<int> default_budgets() {
    std::vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(4);
    v.push_back(8);
    v.push_back(16);
    v.push_back(32);
    v.push_back(64);
    v.push_back(128);
    v.push_back(256);
    v.push_back(512);
    v.push_back(1024);
    v.push_back(2000);
    return v;
}

static double mean_of(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double s = std::accumulate(v.begin(), v.end(), 0.0);
    return s / (double)v.size();
}

static double stddev_of(const std::vector<double>& v) {
    if (v.size() <= 1) return 0.0;
    double m = mean_of(v);
    double acc = 0.0;
    for (size_t i = 0; i < v.size(); ++i) {
        const double d = v[i] - m;
        acc += d * d;
    }
    return std::sqrt(acc / (double)(v.size() - 1));
}

static double ci95_of(const std::vector<double>& v) {
    if (v.size() <= 1) return 0.0;
    return 1.96 * stddev_of(v) / std::sqrt((double)v.size());
}

static double percentile_sorted(const std::vector<double>& sorted, double q) {
    if (sorted.empty()) return 0.0;
    if (sorted.size() == 1) return sorted[0];
    double pos = q * (double)(sorted.size() - 1);
    size_t lo = (size_t)std::floor(pos);
    size_t hi = (size_t)std::ceil(pos);
    if (lo == hi) return sorted[lo];
    double t = pos - (double)lo;
    return sorted[lo] * (1.0 - t) + sorted[hi] * t;
}

static uint64_t seed_for(uint64_t base, int salt1, int salt2, int salt3) {
    uint64_t x = base;
    x ^= (uint64_t)0x9E3779B97F4A7C15ULL * (uint64_t)(salt1 + 1);
    x ^= (uint64_t)0xD1B54A32D192ED03ULL * (uint64_t)(salt2 + 3);
    x ^= (uint64_t)0x94D049BB133111EBULL * (uint64_t)(salt3 + 7);
    return x;
}

static void ensure_review_dirs() {
    ensure_dir_review("data");
    ensure_dir_review("data/review_supp");
}

static std::ofstream open_csv_trunc(const std::string& path, const std::string& header) {
    std::ofstream ofs(path.c_str(), std::ios::out | std::ios::trunc);
    ofs << header;
    ofs.flush();
    return ofs;
}

static size_t meta_bytes(const preauth::RepairMeta& meta) {
    return preauth::repair_meta_payload_bytes(meta);
}


static size_t polyvec_i16_payload_bytes(const common::Params& p) {
    return (size_t)p.k * (size_t)p.N * 2u;
}

static size_t delta_representation_bytes(const common::Params& p, const lattice::PolyVec&) {
    
    
    
    return polyvec_i16_payload_bytes(p);
}

static size_t proof_payload_bytes(const common::Params& p, const zk::RepairSigmaProof& pf) {
    const size_t pv = polyvec_i16_payload_bytes(p);
    size_t n = 0;
    n += 4u; 
    n += 4u; 
    n += 4u; 
    n += 4u + pf.exact_digest.size();
    n += 4u + pf.exact_proof_blob.size();
    n += 4u * pv; 
    n += 2u * pv; 
    n += 4u;      
    n += pf.s_bits.size() * pv;
    return n;
}

static size_t commit_payload_bytes(const common::Params& p, const preauth::CommitMsg& cm) {
    (void)cm;
    size_t n = 0;
    n += polyvec_i16_payload_bytes(p); 
    n += 4u;  
    n += 8u;  
    
    
    
    n += 8u;  
    n += 8u;  
    return n;
}

static size_t authproof_payload_bytes(const common::Params& p, const preauth::AuthProof& pf) {
    const size_t pv = polyvec_i16_payload_bytes(p);
    size_t n = commit_payload_bytes(p, pf.cm);
    n += pv; 
    n += pv; 
    n += pv; 
    n += delta_representation_bytes(p, pf.rsp.delta);
    n += meta_bytes(pf.rsp.meta);
    n += proof_payload_bytes(p, pf.rsp.proof);
    return n;
}

static std::string bytes_hex(const std::vector<uint8_t>& v) {
    static const char* hexd = "0123456789abcdef";
    std::string out;
    out.reserve(v.size() * 2);
    for (size_t i = 0; i < v.size(); ++i) {
        unsigned x = (unsigned)v[i];
        out.push_back(hexd[(x >> 4) & 0xF]);
        out.push_back(hexd[x & 0xF]);
    }
    return out;
}

static bool polyvec_eq_modq_exp(const common::Params& p,
    const lattice::PolyVec& a,
    const lattice::PolyVec& b) {
    if ((int)a.v.size() != p.k || (int)b.v.size() != p.k) return false;
    for (int i = 0; i < p.k; ++i) {
        if ((int)a.v[(size_t)i].c.size() != p.N || (int)b.v[(size_t)i].c.size() != p.N) return false;
        for (int j = 0; j < p.N; ++j) {
            if (lattice::modq((int)a.v[(size_t)i].c[(size_t)j], p.q) !=
                lattice::modq((int)b.v[(size_t)i].c[(size_t)j], p.q)) return false;
        }
    }
    return true;
}

struct RepairPathChecks {
    bool repair_relation_ok = false;
    bool algcheck_raw_ok = false;
    bool algcheck_recovered_ok = false;
    bool ctx_bind_ok = false;
    bool refresh_check_ok = false;
};

static RepairPathChecks eval_repair_path_checks(const common::Params& p,
    const lattice::PublicKey& pk,
    preauth::Verifier& V,
    const preauth::AuthProof& pf) {
    RepairPathChecks out;
    preauth::RepairTheta th{ p.B, p.d };
    std::vector<int> c_bits = V.derive_challenge(pf.cm);
    if (c_bits.empty()) return out;

    out.repair_relation_ok = preauth::check_repair_delta(p, th, pf.rsp.z, pf.rsp.delta, pf.rsp.meta);
    std::vector<uint8_t> expected_ctx = preauth::BuildRepairContextDigest(p, pk.t, pf.cm, c_bits);
    out.ctx_bind_ok = preauth::check_repair_context_binding(pf.rsp.meta, expected_ctx, pf.cm.refresh_tag);
    out.refresh_check_ok = (pf.rsp.meta.refresh_tag == pf.cm.refresh_tag);

    int c = 0;
    for (size_t i = 0; i < c_bits.size(); ++i) c ^= (c_bits[i] & 1);
    if (!preauth::stmt::CheckResidualSmallCentered(p, pf.rsp.r_small)) return out;

    lattice::PolyVec zstar_recovered = preauth::recover_zstar_from_repair(p, pf.rsp.z, pf.rsp.delta);
    lattice::PolyVec y = preauth::stmt::BuildYFromCommit(p, pk, pf.cm, c, pf.rsp.r_small);
    lattice::PolyVec alg_y = lattice::mat_vec_mul(p, pk.A, zstar_recovered);
    out.algcheck_recovered_ok = polyvec_eq_modq_exp(p, alg_y, y);
    
    out.algcheck_raw_ok = out.algcheck_recovered_ok;
    return out;
}

static int maxabs_centered_polyvec(const common::Params& p, const lattice::PolyVec& v) {
    int out = 0;
    for (size_t i = 0; i < v.v.size(); ++i) {
        for (size_t j = 0; j < v.v[i].c.size(); ++j) {
            int x = lattice::center_q((int)v.v[i].c[j], p.q);
            if (std::abs(x) > out) out = std::abs(x);
        }
    }
    return out;
}

static lattice::KeyPair fixed_keypair_for_case(const common::Params& p, int case_id, int salt) {
    common::Rng krng(p.seed ^ ((uint64_t)case_id * 0x9E3779B97F4A7C15ULL) ^ (uint64_t)salt);
    return lattice::keygen(p, krng);
}

static bool run_repair_session(const common::Params& p,
    common::Rng& rng,
    preauth::Prover& P,
    preauth::Verifier& V,
    int& attempts,
    bool& ok,
    double& total_ms,
    double& prove_ms,
    double& verify_ms,
    preauth::ProveObs& obs,
    preauth::AuthProof& pf) {

    attempts = 1;
    common::Timer tp;
    pf = P.prove_nizk_observe(rng, &obs);
    prove_ms = tp.ms();
    common::Timer tv;
    ok = V.verify_nizk(pf);
    verify_ms = tv.ms();
    total_ms = prove_ms + verify_ms;
    if (ok) P.on_accept(pf.cm, pf.rsp);
    return ok;
}

static bool run_abort_session(const common::Params& p,
    common::Rng& rng,
    preauth::Prover& P,
    preauth::Verifier& V,
    int budget,
    int& attempts,
    bool& ok,
    double& total_ms,
    double& prove_ms,
    double& verify_ms,
    preauth::ProveObs& obs_last,
    preauth::AuthProof& pf_last) {

    attempts = 0;
    ok = false;
    prove_ms = 0.0;
    verify_ms = 0.0;
    for (int a = 0; a < budget; ++a) {
        attempts += 1;
        preauth::ProveObs obs;
        preauth::AuthProof pf;
        common::Timer tp;
        pf = P.prove_nizk_observe(rng, &obs);
        prove_ms += tp.ms();
        obs_last = obs;
        pf_last = pf;
    
        
        if (obs.zstar_maxabs_centered > p.B) {
            continue;
        }
    
        common::Timer tv;
        ok = V.verify_nizk(pf);
        verify_ms += tv.ms();
        if (ok) P.on_accept(pf.cm, pf.rsp);
        break;
    }
    total_ms = prove_ms + verify_ms;
    return ok;
}

static SeedAgg finalize_seed_metrics(const std::vector<int>& oks,
    const std::vector<int>& attempts,
    const std::vector<double>& totals) {

    SeedAgg out;
    if (oks.empty()) return out;
    std::vector<double> td = totals;
    std::sort(td.begin(), td.end());
    int ok_sum = 0;
    long long att_sum = 0;
    double total_sum = 0.0;
    for (size_t i = 0; i < oks.size(); ++i) {
        ok_sum += oks[i];
        att_sum += attempts[i];
        total_sum += totals[i];
    }
    out.success_rate = (double)ok_sum / (double)oks.size();
    out.avg_attempts = (double)att_sum / (double)attempts.size();
    out.avg_total_ms = total_sum / (double)totals.size();
    out.median_ms = percentile_sorted(td, 0.50);
    out.p95_ms = percentile_sorted(td, 0.95);
    out.p99_ms = percentile_sorted(td, 0.99);
    out.std_ms = stddev_of(td);
    out.ci95_ms = ci95_of(td);
    return out;
}

static void add_summary(std::map<KeyMS, SummaryAgg>& agg,
    const std::string& profile,
    const char* boundary,
    const char* method,
    int budget,
    int review_sessions,
    const SeedAgg& m) {

    KeyMS key = {"bounded_response", profile, boundary, method, budget};
    SummaryAgg& s = agg[key];
    s.seeds += 1;
    s.sessions_per_seed = review_sessions;
    s.success_rates.push_back(m.success_rate);
    s.attempts.push_back(m.avg_attempts);
    s.totals.push_back(m.avg_total_ms);
    s.medians.push_back(m.median_ms);
    s.p95s.push_back(m.p95_ms);
    s.p99s.push_back(m.p99_ms);
}

static void run_multiseed_bounded_response(const common::Params& base, int review_seeds, int review_sessions) {
    ensure_review_dirs();
    const std::string profile = profile_name(base);
    const uint64_t scope = 0x1111ULL;

    std::ofstream session_ofs = open_csv_trunc(
        out_path(base, "bounded_response_session_level"),
        "experiment,profile,boundary,method,budget,seed,sessions,session_id,success,attempts,total_ms,prove_ms,verify_ms,repair_check_ms,metadata_payload_bytes,delta_representation_bytes,proof_bytes,authproof_bytes,network_payload_bytes,zstar_maxabs,clamp_count,repair_triggered,delta_l0,delta_linf,delta_digest_hex,z_raw_linf,z_repaired_linf,repair_meta_bytes,repair_relation_ok,algcheck_raw_ok,algcheck_recovered_ok,ctx_bind_ok,refresh_check_ok\n");
    std::ofstream seed_ofs = open_csv_trunc(
        out_path(base, "bounded_response_seed_level"),
        "experiment,profile,boundary,method,budget,seed,sessions,success_rate,avg_attempts,avg_total_ms,median_ms,p95_ms,p99_ms,std_ms,ci95_ms\n");
    std::ofstream sum_ofs = open_csv_trunc(
        out_path(base, "bounded_response_summary_ci"),
        "experiment,profile,boundary,method,budget,seeds,sessions_per_seed,mean_success_rate,std_success_rate,ci95_success_rate,mean_attempts,std_attempts,ci95_attempts,mean_total_ms,std_total_ms,ci95_total_ms,median_ms_mean,p95_ms_mean,p99_ms_mean\n");
    
    std::map<KeyMS, SummaryAgg> agg;
    const std::vector<BoundaryCase> cases = default_cases();
    const std::vector<int> budgets = default_budgets();
    
    for (size_t ci = 0; ci < cases.size(); ++ci) {
        std::cout << "[REVIEW] bounded-response case=" << cases[ci].label << ", seeds=" << review_seeds << ", sessions=" << review_sessions << "\n";
        common::Params p = base;
        p.B = cases[ci].B;
        p.eta = cases[ci].eta;
        p.offload_proto = 0;
        p.off_delay_us = 0;
        p.off_delay_jitter_us = 0;
        p.off_delay_step_us = 0;
    
        
        
        lattice::KeyPair kp = fixed_keypair_for_case(p, (int)ci, 0x1111);
    
        for (int si = 0; si < review_seeds; ++si) {
            
            {
                const uint64_t seed_run = seed_for(base.seed, (int)ci, si, 101);
                common::Rng rng(seed_run);
                preauth::Prover P(p, kp.pk, kp.sk);
                preauth::Verifier V(p, kp.pk);
                P.set_scope(scope);
                V.set_scope(scope);
    
                std::vector<int> oks;
                std::vector<int> attempts_vec;
                std::vector<double> totals;
                for (int sess = 0; sess < review_sessions; ++sess) {
                    int attempts = 0;
                    bool ok = false;
                    double total_ms = 0.0;
                    double prove_ms = 0.0;
                    double verify_ms = 0.0;
                    preauth::ProveObs obs;
                    preauth::AuthProof pf;
                    run_repair_session(p, rng, P, V, attempts, ok, total_ms, prove_ms, verify_ms, obs, pf);
                    oks.push_back(ok ? 1 : 0);
                    attempts_vec.push_back(attempts);
                    totals.push_back(total_ms);
                    common::Timer trc;
                    RepairPathChecks chk = eval_repair_path_checks(p, kp.pk, V, pf);
                    const double repair_check_ms = trc.ms();
                    const size_t metadata_payload_bytes = meta_bytes(pf.rsp.meta);
                    const size_t delta_bytes = delta_representation_bytes(p, pf.rsp.delta);
                    const size_t proof_bytes = proof_payload_bytes(p, pf.rsp.proof);
                    const size_t authproof_bytes = authproof_payload_bytes(p, pf);
                    const size_t network_payload_bytes = authproof_bytes;
                    session_ofs << "bounded_response," << profile << "," << cases[ci].label << ",repair,-1,"
                                << std::hex << seed_run << std::dec << "," << review_sessions << "," << sess << ","
                                << (ok ? 1 : 0) << "," << attempts << "," << total_ms << ","
                                << prove_ms << "," << verify_ms << "," << repair_check_ms << ","
                                << metadata_payload_bytes << "," << delta_bytes << "," << proof_bytes << ","
                                << authproof_bytes << "," << network_payload_bytes << ","
                                << obs.zstar_maxabs_centered << "," << obs.clamp_count << ","
                                << (obs.clamp_count > 0 ? 1 : 0) << "," << pf.rsp.meta.delta_l0 << "," << pf.rsp.meta.delta_linf << ","
                                << bytes_hex(pf.rsp.meta.delta_digest) << "," << obs.zstar_maxabs_centered << ","
                                << maxabs_centered_polyvec(p, pf.rsp.z) << "," << meta_bytes(pf.rsp.meta) << ","
                                << (chk.repair_relation_ok ? 1 : 0) << "," << (chk.algcheck_raw_ok ? 1 : 0) << ","
                                << (chk.algcheck_recovered_ok ? 1 : 0) << "," << (chk.ctx_bind_ok ? 1 : 0) << ","
                                << (chk.refresh_check_ok ? 1 : 0) << "\n";
                }
                SeedAgg m = finalize_seed_metrics(oks, attempts_vec, totals);
                seed_ofs << "bounded_response," << profile << "," << cases[ci].label << ",repair,-1,"
                         << std::hex << seed_run << std::dec << "," << review_sessions << ","
                         << m.success_rate << "," << m.avg_attempts << "," << m.avg_total_ms << ","
                         << m.median_ms << "," << m.p95_ms << "," << m.p99_ms << ","
                         << m.std_ms << "," << m.ci95_ms << "\n";
                add_summary(agg, profile, cases[ci].label, "repair", -1, review_sessions, m);
            }
    
            
            for (size_t bi = 0; bi < budgets.size(); ++bi) {
                const int budget = budgets[bi];
                const uint64_t seed_run = seed_for(base.seed, (int)ci, si, 201 + budget);
                common::Rng rng(seed_run);
                preauth::Prover P(p, kp.pk, kp.sk);
                preauth::Verifier V(p, kp.pk);
                P.set_scope(scope);
                V.set_scope(scope);
    
                std::vector<int> oks;
                std::vector<int> attempts_vec;
                std::vector<double> totals;
                for (int sess = 0; sess < review_sessions; ++sess) {
                    int attempts = 0;
                    bool ok = false;
                    double total_ms = 0.0;
                    double prove_ms = 0.0;
                    double verify_ms = 0.0;
                    preauth::ProveObs obs;
                    preauth::AuthProof pf;
                    run_abort_session(p, rng, P, V, budget, attempts, ok, total_ms, prove_ms, verify_ms, obs, pf);
                    oks.push_back(ok ? 1 : 0);
                    attempts_vec.push_back(attempts);
                    totals.push_back(total_ms);
                    common::Timer trc;
                    RepairPathChecks chk = eval_repair_path_checks(p, kp.pk, V, pf);
                    const double repair_check_ms = trc.ms();
                    const size_t metadata_payload_bytes = meta_bytes(pf.rsp.meta);
                    const size_t delta_bytes = delta_representation_bytes(p, pf.rsp.delta);
                    const size_t proof_bytes = proof_payload_bytes(p, pf.rsp.proof);
                    const size_t authproof_bytes = authproof_payload_bytes(p, pf);
                    const size_t network_payload_bytes = authproof_bytes;
                    session_ofs << "bounded_response," << profile << "," << cases[ci].label << ",abort_baseline," << budget << ","
                                << std::hex << seed_run << std::dec << "," << review_sessions << "," << sess << ","
                                << (ok ? 1 : 0) << "," << attempts << "," << total_ms << ","
                                << prove_ms << "," << verify_ms << "," << repair_check_ms << ","
                                << metadata_payload_bytes << "," << delta_bytes << "," << proof_bytes << ","
                                << authproof_bytes << "," << network_payload_bytes << ","
                                << obs.zstar_maxabs_centered << "," << obs.clamp_count << ","
                                << (obs.clamp_count > 0 ? 1 : 0) << "," << pf.rsp.meta.delta_l0 << "," << pf.rsp.meta.delta_linf << ","
                                << bytes_hex(pf.rsp.meta.delta_digest) << "," << obs.zstar_maxabs_centered << ","
                                << maxabs_centered_polyvec(p, pf.rsp.z) << "," << meta_bytes(pf.rsp.meta) << ","
                                << (chk.repair_relation_ok ? 1 : 0) << "," << (chk.algcheck_raw_ok ? 1 : 0) << ","
                                << (chk.algcheck_recovered_ok ? 1 : 0) << "," << (chk.ctx_bind_ok ? 1 : 0) << ","
                                << (chk.refresh_check_ok ? 1 : 0) << "\n";
                }
                SeedAgg m = finalize_seed_metrics(oks, attempts_vec, totals);
                seed_ofs << "bounded_response," << profile << "," << cases[ci].label << ",abort_baseline," << budget << ","
                         << std::hex << seed_run << std::dec << "," << review_sessions << ","
                         << m.success_rate << "," << m.avg_attempts << "," << m.avg_total_ms << ","
                         << m.median_ms << "," << m.p95_ms << "," << m.p99_ms << ","
                         << m.std_ms << "," << m.ci95_ms << "\n";
                add_summary(agg, profile, cases[ci].label, "abort_baseline", budget, review_sessions, m);
            }
            session_ofs.flush();
            seed_ofs.flush();
        }
    }
    
    for (std::map<KeyMS, SummaryAgg>::const_iterator it = agg.begin(); it != agg.end(); ++it) {
        const KeyMS& k = it->first;
        const SummaryAgg& s = it->second;
        sum_ofs << k.experiment << "," << k.profile << "," << k.boundary << "," << k.method << "," << k.budget << ","
                << s.seeds << "," << s.sessions_per_seed << ","
                << mean_of(s.success_rates) << "," << stddev_of(s.success_rates) << "," << ci95_of(s.success_rates) << ","
                << mean_of(s.attempts) << "," << stddev_of(s.attempts) << "," << ci95_of(s.attempts) << ","
                << mean_of(s.totals) << "," << stddev_of(s.totals) << "," << ci95_of(s.totals) << ","
                << mean_of(s.medians) << "," << mean_of(s.p95s) << "," << mean_of(s.p99s) << "\n";
    }
    
    std::cout << "[REVIEW] wrote " << out_path(base, "bounded_response_session_level") << "\n";
    std::cout << "[REVIEW] wrote " << out_path(base, "bounded_response_seed_level") << "\n";
    std::cout << "[REVIEW] wrote " << out_path(base, "bounded_response_summary_ci") << "\n";
}

static int sample_raw_response_norm_fast(const common::Params& p, common::Rng& rng, const lattice::SecretKey& sk) {
    
    
    
    
    
    
    lattice::PolyVec z = lattice::vec_small(p, rng);
    const int c = rng.bit();
    if (c == 1) {
        for (int i = 0; i < p.k; ++i) {
            for (int j = 0; j < p.N; ++j) {
                z.v[i].c[j] += sk.s.v[i].c[j];
            }
        }
    }
    return maxabs_centered_polyvec(p, z);
}

static void run_accept_probability_one_case(std::ofstream& ofs,
    const common::Params& base,
    const std::string& profile,
    const std::string& sweep_label,
    int B,
    int eta,
    int review_seeds,
    int review_trials,
    int seed_salt) {

    for (int si = 0; si < review_seeds; ++si) {
        common::Params p = base;
        p.B = B;
        p.eta = eta;
        p.offload_proto = 0;
        p.off_delay_us = 0;
        p.off_delay_jitter_us = 0;
        p.off_delay_step_us = 0;
    
        lattice::KeyPair kp = fixed_keypair_for_case(p, seed_salt + B + eta, 0x2222);
        const uint64_t seed_run = seed_for(base.seed, si, B, 401 + eta + seed_salt);
        common::Rng rng(seed_run);
    
        int acc = 0;
        std::vector<double> norms;
        norms.reserve((size_t)review_trials);
        for (int t = 0; t < review_trials; ++t) {
            const int zstar_norm = sample_raw_response_norm_fast(p, rng, kp.sk);
            norms.push_back((double)zstar_norm);
            if (zstar_norm <= p.B) {
                acc += 1;
            }
        }
        std::sort(norms.begin(), norms.end());
        const double pacc = (review_trials > 0 ? ((double)acc / (double)review_trials) : 0.0);
        const double exp_attempts = (pacc > 0.0 ? (1.0 / pacc) : -1.0);
        ofs << "abort_accept_probability," << profile << "," << sweep_label << "," << std::hex << seed_run << std::dec << "," << review_trials << ","
            << p.B << "," << p.eta << "," << pacc << "," << exp_attempts << "," << (acc == 0 ? 1 : 0) << ","
            << mean_of(norms) << "," << percentile_sorted(norms, 0.50) << "," << percentile_sorted(norms, 0.95) << "," << percentile_sorted(norms, 0.99) << "\n";
    }
}

static void run_abort_accept_probability_sweep(const common::Params& base, int review_seeds, int review_trials) {
    ensure_review_dirs();
    const std::string profile = profile_name(base);
    std::ofstream ofs = open_csv_trunc(
        out_path(base, "abort_accept_probability"),
        "experiment,profile,sweep,seed,trials,B,eta,empirical_accept_prob,expected_attempts,zero_success_trials,max_norm_mean,max_norm_median,max_norm_p95,max_norm_p99\n");

    std::cout << "[REVIEW] accept-probability audit uses fast raw-response sampler; trials=" << review_trials << ", seeds=" << review_seeds << "\n";
    
    
    const std::vector<BoundaryCase> cases = default_cases();
    for (size_t ci = 0; ci < cases.size(); ++ci) {
        run_accept_probability_one_case(ofs, base, profile, std::string("anchor_") + cases[ci].label,
            cases[ci].B, cases[ci].eta, review_seeds, review_trials, 1000 + (int)ci);
    }
    
    
    const std::vector<int> Bs = std::vector<int>{2,3,4,5,6,8,10,12};
    for (size_t bi = 0; bi < Bs.size(); ++bi) {
        run_accept_probability_one_case(ofs, base, profile, "B_sweep_eta6",
            Bs[bi], 6, review_seeds, review_trials, 2000 + (int)bi);
    }
    
    std::cout << "[REVIEW] wrote " << out_path(base, "abort_accept_probability") << "\n";
}

static void run_repair_metadata_leakage_audit(const common::Params& base, int review_seeds, int review_sessions) {
    ensure_review_dirs();
    const std::string profile = profile_name(base);
    std::ofstream sess_ofs = open_csv_trunc(
        out_path(base, "repair_metadata_leakage_session"),
        "experiment,profile,boundary,seed,session_id,success,repaired_count,repaired_ratio,metadata_bytes,raw_linf,repaired_linf,delta_l0,delta_linf,delta_digest_hex,repair_relation_ok,algcheck_recovered_ok,ctx_bind_ok,refresh_check_ok\n");
    std::ofstream sum_ofs = open_csv_trunc(
        out_path(base, "repair_metadata_leakage_summary"),
        "experiment,profile,boundary,seeds,sessions,mean_repaired_count,std_repaired_count,ci95_repaired_count,mean_repaired_ratio,mean_metadata_bytes,mean_raw_linf,mean_repaired_linf,max_repaired_linf\n");

    std::map<std::string, std::vector<double> > repaired_counts;
    std::map<std::string, std::vector<double> > repaired_ratios;
    std::map<std::string, std::vector<double> > metadata_bytess;
    std::map<std::string, std::vector<double> > raw_linfs;
    std::map<std::string, std::vector<double> > repaired_linfs;
    std::map<std::string, int> max_repaired_linf;
    
    const std::vector<BoundaryCase> cases = default_cases();
    for (size_t ci = 0; ci < cases.size(); ++ci) {
        std::cout << "[REVIEW] metadata-leakage case=" << cases[ci].label << ", seeds=" << review_seeds << ", sessions=" << review_sessions << "\n";
        common::Params p = base;
        p.B = cases[ci].B;
        p.eta = cases[ci].eta;
        p.offload_proto = 0;
        p.off_delay_us = 0;
        p.off_delay_jitter_us = 0;
        p.off_delay_step_us = 0;
    
        lattice::KeyPair kp = fixed_keypair_for_case(p, (int)ci, 0x3333);
        std::string boundary = cases[ci].label;
        for (int si = 0; si < review_seeds; ++si) {
            const uint64_t seed_run = seed_for(base.seed, (int)ci, si, 601);
            common::Rng rng(seed_run);
            preauth::Prover P(p, kp.pk, kp.sk);
            preauth::Verifier V(p, kp.pk);
            P.set_scope(0x1111ULL);
            V.set_scope(0x1111ULL);
    
            const double denom = (double)(p.k * p.N);
            for (int sess = 0; sess < review_sessions; ++sess) {
                preauth::ProveObs obs;
                preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);
                bool ok = V.verify_nizk(pf);
                if (ok) P.on_accept(pf.cm, pf.rsp);
    
                const int repaired_count = pf.rsp.meta.clamp_count;
                const double repaired_ratio = denom > 0.0 ? ((double)repaired_count / denom) : 0.0;
                const int repaired_linf = maxabs_centered_polyvec(p, pf.rsp.z);
                const int raw_linf = obs.zstar_maxabs_centered;
                const size_t mbytes = meta_bytes(pf.rsp.meta);
    
                RepairPathChecks chk = eval_repair_path_checks(p, kp.pk, V, pf);
                sess_ofs << "repair_metadata_leakage," << profile << "," << boundary << "," << std::hex << seed_run << std::dec << ","
                         << sess << "," << (ok ? 1 : 0) << "," << repaired_count << "," << repaired_ratio << ","
                         << mbytes << "," << raw_linf << "," << repaired_linf << ","
                         << pf.rsp.meta.delta_l0 << "," << pf.rsp.meta.delta_linf << "," << bytes_hex(pf.rsp.meta.delta_digest) << ","
                         << (chk.repair_relation_ok ? 1 : 0) << "," << (chk.algcheck_recovered_ok ? 1 : 0) << ","
                         << (chk.ctx_bind_ok ? 1 : 0) << "," << (chk.refresh_check_ok ? 1 : 0) << "\n";
    
                repaired_counts[boundary].push_back((double)repaired_count);
                repaired_ratios[boundary].push_back(repaired_ratio);
                metadata_bytess[boundary].push_back((double)mbytes);
                raw_linfs[boundary].push_back((double)raw_linf);
                repaired_linfs[boundary].push_back((double)repaired_linf);
                if (repaired_linf > max_repaired_linf[boundary]) {
                    max_repaired_linf[boundary] = repaired_linf;
                }
            }
            sess_ofs.flush();
        }
    }
    
    for (size_t ci = 0; ci < cases.size(); ++ci) {
        std::string boundary = cases[ci].label;
        sum_ofs << "repair_metadata_leakage," << profile << "," << boundary << "," << review_seeds << "," << review_sessions << ","
                << mean_of(repaired_counts[boundary]) << "," << stddev_of(repaired_counts[boundary]) << "," << ci95_of(repaired_counts[boundary]) << ","
                << mean_of(repaired_ratios[boundary]) << "," << mean_of(metadata_bytess[boundary]) << ","
                << mean_of(raw_linfs[boundary]) << "," << mean_of(repaired_linfs[boundary]) << "," << max_repaired_linf[boundary] << "\n";
    }
    std::cout << "[REVIEW] wrote " << out_path(base, "repair_metadata_leakage_session") << "\n";
    std::cout << "[REVIEW] wrote " << out_path(base, "repair_metadata_leakage_summary") << "\n";
}


static lattice::PolyVec polyvec_to_modq_exp(const common::Params& p, const lattice::PolyVec& x) {
    lattice::PolyVec out = lattice::vec_zero(p);
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j) {
            out.v[(size_t)i].c[(size_t)j] = (lattice::Poly::coeff_t)lattice::modq((int)x.v[(size_t)i].c[(size_t)j], p.q);
        }
    }
    return out;
}

static bool bound_ok_exp(const common::Params& p, const lattice::PolyVec& z) {
    return maxabs_centered_polyvec(p, z) <= p.B;
}

static bool algcheck_on_response_exp(const common::Params& p,
    const lattice::PublicKey& pk,
    preauth::Verifier& V,
    const preauth::AuthProof& pf,
    const lattice::PolyVec& response) {

    std::vector<int> c_bits = V.derive_challenge(pf.cm);
    if (c_bits.empty()) return false;

    int c = 0;
    for (size_t i = 0; i < c_bits.size(); ++i) c ^= (c_bits[i] & 1);
    if (!preauth::stmt::CheckResidualSmallCentered(p, pf.rsp.r_small)) return false;

    lattice::PolyVec y = preauth::stmt::BuildYFromCommit(p, pk, pf.cm, c, pf.rsp.r_small);
    lattice::PolyVec w = polyvec_to_modq_exp(p, response);
    lattice::PolyVec alg_y = lattice::mat_vec_mul(p, pk.A, w);
    return polyvec_eq_modq_exp(p, alg_y, y);
}

struct BindingAblationChecks {
    bool bound_ok = false;
    bool repair_relation_ok = false;
    bool residual_ok = false;
    bool algcheck_recovered_ok = false;
    bool ctx_bind_ok = false;
    bool refresh_check_ok = false;

    bool full_accept = false;
    bool no_context_accept = false;
    bool no_refresh_accept = false;
    bool no_context_no_refresh_accept = false;
};

static BindingAblationChecks eval_binding_ablation_checks(const common::Params& p,
    const lattice::PublicKey& pk,
    preauth::Verifier& V,
    const preauth::AuthProof& pf) {

    BindingAblationChecks out;
    preauth::RepairTheta th{ p.B, p.d };

    std::vector<int> c_bits = V.derive_challenge(pf.cm);
    if (c_bits.empty()) return out;

    out.bound_ok = bound_ok_exp(p, pf.rsp.z);
    out.repair_relation_ok = preauth::check_repair_delta(p, th, pf.rsp.z, pf.rsp.delta, pf.rsp.meta);
    out.residual_ok = preauth::stmt::CheckResidualSmallCentered(p, pf.rsp.r_small);

    lattice::PolyVec zstar = preauth::recover_zstar_from_repair(p, pf.rsp.z, pf.rsp.delta);
    out.algcheck_recovered_ok = algcheck_on_response_exp(p, pk, V, pf, zstar);

    std::vector<uint8_t> expected_ctx = preauth::BuildRepairContextDigest(p, pk.t, pf.cm, c_bits);
    out.ctx_bind_ok = (pf.rsp.meta.ctx_digest == expected_ctx);
    out.refresh_check_ok = (pf.rsp.meta.refresh_tag == pf.cm.refresh_tag);

    const bool core_ok = out.bound_ok && out.repair_relation_ok && out.residual_ok && out.algcheck_recovered_ok;
    out.full_accept = core_ok && out.ctx_bind_ok && out.refresh_check_ok;
    out.no_context_accept = core_ok && out.refresh_check_ok;
    out.no_refresh_accept = core_ok && out.ctx_bind_ok;
    out.no_context_no_refresh_accept = core_ok;
    return out;
}

static preauth::AuthProof rebind_meta_for_ablation(const common::Params& p,
    const preauth::AuthProof& pf,
    const std::vector<uint8_t>& ctx_digest,
    uint64_t refresh_tag) {

    preauth::AuthProof x = pf;
    preauth::RepairTheta th{ p.B, p.d };
    preauth::bind_repair_meta(p, th, x.rsp.z, x.rsp.delta, ctx_digest, refresh_tag, x.rsp.meta);
    return x;
}

static void run_local_clipping_baseline(const common::Params& base, int review_seeds, int review_sessions) {
    ensure_review_dirs();
    const std::string profile = profile_name(base);
    std::ofstream ofs = open_csv_trunc(
        out_path(base, "local_clipping_baseline_session"),
        "experiment,profile,boundary,seed,session_id,full_repair_accept,repaired,local_clip_bound_ok,local_clip_algcheck_z_ok,algcheck_zplus_delta_ok,repair_relation_ok,ctx_bind_ok,refresh_check_ok,delta_l0,delta_linf,z_raw_linf,z_repaired_linf,metadata_bytes\n");
    std::ofstream sum_ofs = open_csv_trunc(
        out_path(base, "local_clipping_baseline_summary"),
        "experiment,profile,boundary,seeds,sessions,mean_repaired,mean_bound_ok,mean_algcheck_z_ok,mean_algcheck_zplus_delta_ok,mean_full_repair_accept\n");

    std::map<std::string, std::vector<double> > repaired_v;
    std::map<std::string, std::vector<double> > bound_v;
    std::map<std::string, std::vector<double> > alg_z_v;
    std::map<std::string, std::vector<double> > alg_rec_v;
    std::map<std::string, std::vector<double> > full_v;

    const std::vector<BoundaryCase> cases = default_cases();
    for (size_t ci = 0; ci < cases.size(); ++ci) {
        common::Params p = base;
        p.B = cases[ci].B;
        p.eta = cases[ci].eta;
        p.offload_proto = 0;
        p.off_delay_us = 0;
        p.off_delay_jitter_us = 0;
        p.off_delay_step_us = 0;
        const std::string boundary = cases[ci].label;
        lattice::KeyPair kp = fixed_keypair_for_case(p, (int)ci, 0x5555);

        std::cout << "[REVIEW] local-clipping baseline case=" << boundary << ", seeds=" << review_seeds << ", sessions=" << review_sessions << "\n";

        for (int si = 0; si < review_seeds; ++si) {
            const uint64_t seed_run = seed_for(base.seed, (int)ci, si, 901);
            common::Rng rng(seed_run);
            preauth::Prover P(p, kp.pk, kp.sk);
            preauth::Verifier V(p, kp.pk);
            P.set_scope(0x1111ULL);
            V.set_scope(0x1111ULL);

            for (int sess = 0; sess < review_sessions; ++sess) {
                preauth::ProveObs obs;
                preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);

                const bool repaired = (pf.rsp.meta.clamp_count > 0);
                const bool b_ok = bound_ok_exp(p, pf.rsp.z);
                const bool alg_z_ok = algcheck_on_response_exp(p, kp.pk, V, pf, pf.rsp.z);
                lattice::PolyVec zstar = preauth::recover_zstar_from_repair(p, pf.rsp.z, pf.rsp.delta);
                const bool alg_rec_ok = algcheck_on_response_exp(p, kp.pk, V, pf, zstar);
                RepairPathChecks chk = eval_repair_path_checks(p, kp.pk, V, pf);

                
                
                
                
                const bool full_ok = b_ok && alg_rec_ok && chk.repair_relation_ok && chk.ctx_bind_ok && chk.refresh_check_ok;
                if (full_ok) P.on_accept(pf.cm, pf.rsp);

                ofs << "local_clipping_baseline," << profile << "," << boundary << "," << std::hex << seed_run << std::dec << ","
                    << sess << "," << (full_ok ? 1 : 0) << "," << (repaired ? 1 : 0) << ","
                    << (b_ok ? 1 : 0) << "," << (alg_z_ok ? 1 : 0) << "," << (alg_rec_ok ? 1 : 0) << ","
                    << (chk.repair_relation_ok ? 1 : 0) << "," << (chk.ctx_bind_ok ? 1 : 0) << "," << (chk.refresh_check_ok ? 1 : 0) << ","
                    << pf.rsp.meta.delta_l0 << "," << pf.rsp.meta.delta_linf << ","
                    << obs.zstar_maxabs_centered << "," << maxabs_centered_polyvec(p, pf.rsp.z) << "," << meta_bytes(pf.rsp.meta) << "\n";

                repaired_v[boundary].push_back(repaired ? 1.0 : 0.0);
                bound_v[boundary].push_back(b_ok ? 1.0 : 0.0);
                alg_z_v[boundary].push_back(alg_z_ok ? 1.0 : 0.0);
                alg_rec_v[boundary].push_back(alg_rec_ok ? 1.0 : 0.0);
                full_v[boundary].push_back(full_ok ? 1.0 : 0.0);
            }
            ofs.flush();
        }
    }

    for (size_t ci = 0; ci < cases.size(); ++ci) {
        const std::string boundary = cases[ci].label;
        sum_ofs << "local_clipping_baseline," << profile << "," << boundary << "," << review_seeds << "," << review_sessions << ","
                << mean_of(repaired_v[boundary]) << "," << mean_of(bound_v[boundary]) << ","
                << mean_of(alg_z_v[boundary]) << "," << mean_of(alg_rec_v[boundary]) << "," << mean_of(full_v[boundary]) << "\n";
    }

    std::cout << "[REVIEW] wrote " << out_path(base, "local_clipping_baseline_session") << "\n";
    std::cout << "[REVIEW] wrote " << out_path(base, "local_clipping_baseline_summary") << "\n";
}

static void run_binding_ablation_controls(const common::Params& base, int review_seeds, int review_sessions) {
    ensure_review_dirs();
    const std::string profile = profile_name(base);
    std::ofstream ofs = open_csv_trunc(
        out_path(base, "binding_ablation_session"),
        "experiment,profile,boundary,seed,session_id,case_name,full_accept,no_context_accept,no_refresh_accept,no_context_no_refresh_accept,bound_ok,repair_relation_ok,algcheck_recovered_ok,ctx_bind_ok,refresh_check_ok\n");
    std::ofstream sum_ofs = open_csv_trunc(
        out_path(base, "binding_ablation_summary"),
        "experiment,profile,boundary,case_name,seeds,sessions,mean_full_accept,mean_no_context_accept,mean_no_refresh_accept,mean_no_context_no_refresh_accept\n");

    struct CaseAgg {
        std::vector<double> full;
        std::vector<double> no_ctx;
        std::vector<double> no_refresh;
        std::vector<double> no_both;
    };
    std::map<std::string, CaseAgg> agg;

    const std::vector<BoundaryCase> cases = default_cases();
    for (size_t ci = 0; ci < cases.size(); ++ci) {
        common::Params p = base;
        p.B = cases[ci].B;
        p.eta = cases[ci].eta;
        p.offload_proto = 0;
        p.off_delay_us = 0;
        p.off_delay_jitter_us = 0;
        p.off_delay_step_us = 0;
        const std::string boundary = cases[ci].label;
        lattice::KeyPair kp = fixed_keypair_for_case(p, (int)ci, 0x6666);

        std::cout << "[REVIEW] binding-ablation case=" << boundary << ", seeds=" << review_seeds << ", sessions=" << review_sessions << "\n";

        for (int si = 0; si < review_seeds; ++si) {
            const uint64_t seed_run = seed_for(base.seed, (int)ci, si, 1001);
            common::Rng rng(seed_run);
            preauth::Prover P(p, kp.pk, kp.sk);
            preauth::Verifier V(p, kp.pk);
            P.set_scope(0x1111ULL);
            V.set_scope(0x1111ULL);

            for (int sess = 0; sess < review_sessions; ++sess) {
                preauth::ProveObs obs;
                (void)obs;
                preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);

                
                bool ok_valid = V.verify_nizk(pf);
                if (ok_valid) P.on_accept(pf.cm, pf.rsp);

                std::vector<int> c_bits = V.derive_challenge(pf.cm);
                std::vector<uint8_t> expected_ctx = preauth::BuildRepairContextDigest(p, kp.pk.t, pf.cm, c_bits);
                std::vector<uint8_t> fake_ctx(32, (uint8_t)(0xA0 + (int)ci));

                preauth::AuthProof variants[4];
                const char* names[4] = {
                    "valid",
                    "ctx_digest_rebound_wrong",
                    "refresh_tag_rebound_stale",
                    "ctx_and_refresh_rebound_wrong"
                };

                variants[0] = pf;
                variants[1] = rebind_meta_for_ablation(p, pf, fake_ctx, pf.cm.refresh_tag);
                variants[2] = rebind_meta_for_ablation(p, pf, expected_ctx, pf.cm.refresh_tag ^ 0x55AA55AAULL);
                variants[3] = rebind_meta_for_ablation(p, pf, fake_ctx, pf.cm.refresh_tag ^ 0x55AA55AAULL);

                for (int vi = 0; vi < 4; ++vi) {
                    BindingAblationChecks chk = eval_binding_ablation_checks(p, kp.pk, V, variants[vi]);
                    ofs << "binding_ablation," << profile << "," << boundary << "," << std::hex << seed_run << std::dec << ","
                        << sess << "," << names[vi] << ","
                        << (chk.full_accept ? 1 : 0) << "," << (chk.no_context_accept ? 1 : 0) << ","
                        << (chk.no_refresh_accept ? 1 : 0) << "," << (chk.no_context_no_refresh_accept ? 1 : 0) << ","
                        << (chk.bound_ok ? 1 : 0) << "," << (chk.repair_relation_ok ? 1 : 0) << ","
                        << (chk.algcheck_recovered_ok ? 1 : 0) << "," << (chk.ctx_bind_ok ? 1 : 0) << ","
                        << (chk.refresh_check_ok ? 1 : 0) << "\n";

                    const std::string key = boundary + std::string("|") + names[vi];
                    agg[key].full.push_back(chk.full_accept ? 1.0 : 0.0);
                    agg[key].no_ctx.push_back(chk.no_context_accept ? 1.0 : 0.0);
                    agg[key].no_refresh.push_back(chk.no_refresh_accept ? 1.0 : 0.0);
                    agg[key].no_both.push_back(chk.no_context_no_refresh_accept ? 1.0 : 0.0);
                }
            }
            ofs.flush();
        }
    }

    for (size_t ci = 0; ci < cases.size(); ++ci) {
        const std::string boundary = cases[ci].label;
        const char* names[4] = {
            "valid",
            "ctx_digest_rebound_wrong",
            "refresh_tag_rebound_stale",
            "ctx_and_refresh_rebound_wrong"
        };
        for (int vi = 0; vi < 4; ++vi) {
            const std::string key = boundary + std::string("|") + names[vi];
            CaseAgg& a = agg[key];
            sum_ofs << "binding_ablation," << profile << "," << boundary << "," << names[vi] << ","
                    << review_seeds << "," << review_sessions << ","
                    << mean_of(a.full) << "," << mean_of(a.no_ctx) << ","
                    << mean_of(a.no_refresh) << "," << mean_of(a.no_both) << "\n";
        }
    }

    std::cout << "[REVIEW] wrote " << out_path(base, "binding_ablation_session") << "\n";
    std::cout << "[REVIEW] wrote " << out_path(base, "binding_ablation_summary") << "\n";
}

static void run_context_mismatch_checks(const common::Params& base, int review_seeds, int review_sessions) {
    ensure_review_dirs();
    const std::string profile = profile_name(base);
    std::ofstream ofs = open_csv_trunc(
        out_path(base, "context_mismatch_checks"),
        "experiment,profile,boundary,seed,case_name,trials,accept_count,reject_count,accept_rate,reject_rate\n");

    const std::vector<BoundaryCase> cases = default_cases();
    for (size_t ci = 0; ci < cases.size(); ++ci) {
        common::Params p = base;
        p.B = cases[ci].B;
        p.eta = cases[ci].eta;
        p.offload_proto = 0;
        p.off_delay_us = 0;
        p.off_delay_jitter_us = 0;
        p.off_delay_step_us = 0;
        const std::string boundary = cases[ci].label;
    
        lattice::KeyPair kp = fixed_keypair_for_case(p, (int)ci, 0x4444);
        for (int si = 0; si < review_seeds; ++si) {
            const uint64_t seed_run = seed_for(base.seed, (int)ci, si, 801);
            common::Rng rng(seed_run);
            preauth::Prover P(p, kp.pk, kp.sk);
            preauth::Verifier V(p, kp.pk);
            P.set_scope(0x1111ULL);
            V.set_scope(0x1111ULL);
    
            const char* cases_name[] = {
                "valid",
                "stale_refresh_tag",
                "wrong_scope",
                "wrong_pseudonym",
                "transcript_challenge_mismatch",
                "replayed_metadata",
                "metadata_from_other_session",
                "repaired_response_from_other_session",
                "delta_from_other_session",
                "response_commit_mismatch"
            };
            const int K = (int)(sizeof(cases_name) / sizeof(cases_name[0]));
            std::vector<int> accept(K, 0);
            std::vector<int> reject(K, 0);
    
            for (int t = 0; t < review_sessions; ++t) {
                preauth::ProveObs obs_a, obs_b;
                preauth::AuthProof pf_a = P.prove_nizk_observe(rng, &obs_a);
                preauth::AuthProof pf_b = P.prove_nizk_observe(rng, &obs_b);
    
                {
                    bool ok = V.verify_nizk(pf_a);
                    if (ok) P.on_accept(pf_a.cm, pf_a.rsp);
                    if (ok) accept[0]++; else reject[0]++;
                }
                {
                    preauth::AuthProof x = pf_a;
                    x.cm.refresh_tag ^= 1ULL;
                    bool ok = V.verify_nizk(x);
                    if (ok) accept[1]++; else reject[1]++;
                }
                {
                    preauth::AuthProof x = pf_a;
                    x.cm.scope_tag ^= 1ULL;
                    bool ok = V.verify_nizk(x);
                    if (ok) accept[2]++; else reject[2]++;
                }
                {
                    preauth::AuthProof x = pf_a;
                    x.cm.pseudonym ^= 1ULL;
                    bool ok = V.verify_nizk(x);
                    if (ok) accept[3]++; else reject[3]++;
                }
                {
                    preauth::AuthProof x = pf_a;
                    x.cm.nonce += 1U;
                    bool ok = V.verify_nizk(x);
                    if (ok) accept[4]++; else reject[4]++;
                }
                {
                    preauth::AuthProof x = pf_a;
                    if (!x.rsp.meta.high.empty()) {
                        x.rsp.meta.high[0] = (int16_t)(x.rsp.meta.high[0] + 1);
                    } else {
                        x.rsp.meta.clamp_count += 1;
                    }
                    bool ok = V.verify_nizk(x);
                    if (ok) accept[5]++; else reject[5]++;
                }
                {
                    preauth::AuthProof x = pf_a;
                    x.rsp.meta = pf_b.rsp.meta;
                    bool ok = V.verify_nizk(x);
                    if (ok) accept[6]++; else reject[6]++;
                }
                {
                    preauth::AuthProof x = pf_a;
                    x.rsp.z = pf_b.rsp.z;
                    bool ok = V.verify_nizk(x);
                    if (ok) accept[7]++; else reject[7]++;
                }
                {
                    preauth::AuthProof x = pf_a;
                    x.rsp.delta = pf_b.rsp.delta;
                    bool ok = V.verify_nizk(x);
                    if (ok) accept[8]++; else reject[8]++;
                }
                {
                    preauth::AuthProof x = pf_a;
                    x.rsp.r_small = pf_b.rsp.r_small;
                    bool ok = V.verify_nizk(x);
                    if (ok) accept[9]++; else reject[9]++;
                }
            }
    
            for (int i = 0; i < K; ++i) {
                int trials = accept[i] + reject[i];
                double ar = trials > 0 ? ((double)accept[i] / (double)trials) : 0.0;
                double rr = trials > 0 ? ((double)reject[i] / (double)trials) : 0.0;
                ofs << "context_mismatch," << profile << "," << boundary << "," << std::hex << seed_run << std::dec << ","
                    << cases_name[i] << "," << trials << "," << accept[i] << "," << reject[i] << "," << ar << "," << rr << "\n";
            }
            ofs.flush();
        }
    }
    std::cout << "[REVIEW] wrote " << out_path(base, "context_mismatch_checks") << "\n";
}

} 

void run_exp_review_supp(const common::Params& base,
    int review_seeds,
    int review_sessions,
    int review_trials,
    int do_multiseed,
    int do_accept_prob,
    int do_metadata_leak,
    int do_context_mismatch) {

    const int run_all = (!do_multiseed && !do_accept_prob && !do_metadata_leak && !do_context_mismatch) ? 1 : 0;
    std::cout << "[REVIEW] output profile prefix: " << profile_name(base) << "\n";
    if (run_all || do_multiseed) {
        run_multiseed_bounded_response(base, review_seeds, review_sessions);
    }
    if (run_all || do_accept_prob) {
        run_abort_accept_probability_sweep(base, review_seeds, review_trials);
    }
    if (run_all || do_metadata_leak) {
        run_repair_metadata_leakage_audit(base, review_seeds, review_sessions);
    }
    if (run_all || do_context_mismatch) {
        run_context_mismatch_checks(base, review_seeds, review_sessions);
        run_local_clipping_baseline(base, review_seeds, review_sessions);
        run_binding_ablation_controls(base, review_seeds, review_sessions);
    }
}

} 
