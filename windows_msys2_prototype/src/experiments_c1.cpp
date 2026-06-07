#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <iostream>

#ifdef _WIN32
#include <direct.h>
static void ensure_dir_c1(const char* name) { _mkdir(name); }
#else
#include <sys/stat.h>
#include <sys/types.h>
static void ensure_dir_c1(const char* name) { mkdir(name, 0755); }
#endif

#include "common/params.h"
#include "common/log.h"
#include "common/rng.h"
#include "common/timing.h"

#include "lattice/module_lwe.h"
#include "preauth/preauth.h"

namespace {

	static std::string now_ymdhms_c1() {
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

	static std::string hex_noprefix_u64_c1(uint64_t x) {
		std::ostringstream oss;
		oss << std::hex << std::nouppercase << x;
		return oss.str();
	}
	
	static size_t bytes_polyvec_i16(const lattice::PolyVec& v) {
		size_t coeff_cnt = 0;
		for (size_t i = 0; i < v.v.size(); i++) {
			coeff_cnt += v.v[i].c.size();
		}
		return coeff_cnt * sizeof(int16_t);
	}
	
	static size_t bytes_repair_meta(const preauth::RepairMeta& meta) {
		return meta.high.size() * sizeof(int16_t)
			+ meta.low.size() * sizeof(int16_t)
			+ sizeof(int32_t);
	}
	
	template <typename T>
	struct has_scope_tag {
	private:
		template <typename U>
		static auto test(int) -> decltype((void)std::declval<U>().scope_tag, std::true_type());
		template <typename>
		static std::false_type test(...);
	public:
		static const bool value = decltype(test<T>(0))::value;
	};
	
	template <typename T>
	struct has_pseudonym {
	private:
		template <typename U>
		static auto test(int) -> decltype((void)std::declval<U>().pseudonym, std::true_type());
		template <typename>
		static std::false_type test(...);
	public:
		static const bool value = decltype(test<T>(0))::value;
	};
	
	template <typename T>
	typename std::enable_if<has_scope_tag<T>::value, size_t>::type extra_scope_tag_bytes(const T&) {
		return sizeof(uint64_t);
	}
	
	template <typename T>
	typename std::enable_if<!has_scope_tag<T>::value, size_t>::type extra_scope_tag_bytes(const T&) {
		return 0;
	}
	
	template <typename T>
	typename std::enable_if<has_pseudonym<T>::value, size_t>::type extra_pseudonym_bytes(const T&) {
		return sizeof(uint64_t);
	}
	
	template <typename T>
	typename std::enable_if<!has_pseudonym<T>::value, size_t>::type extra_pseudonym_bytes(const T&) {
		return 0;
	}
	
	static size_t bytes_commit_msg(const preauth::CommitMsg& cm) {
		return bytes_polyvec_i16(cm.w)
			+ sizeof(uint32_t)
			+ sizeof(uint64_t)
			+ extra_scope_tag_bytes(cm)
			+ extra_pseudonym_bytes(cm);
	}
	
	static size_t bytes_repair_sigma_proof(const zk::RepairSigmaProof& pf) {
		size_t out = 0;
		out += sizeof(uint32_t); 
		out += sizeof(uint32_t); 
		out += sizeof(uint32_t); 
		out += sizeof(uint32_t); 
		out += sizeof(uint32_t); 
		out += sizeof(uint32_t); 
		out += sizeof(uint32_t); 
		out += pf.relation_digest.size();
		out += pf.exact_digest.size();
		out += pf.exact_proof_blob.size();
	
		out += bytes_polyvec_i16(pf.a1);
		out += bytes_polyvec_i16(pf.a2);
		out += bytes_polyvec_i16(pf.a3);
		out += bytes_polyvec_i16(pf.a4);
	
		out += bytes_polyvec_i16(pf.s_z);
		out += bytes_polyvec_i16(pf.s_r);
	
		for (size_t i = 0; i < pf.s_bits.size(); i++) {
			out += bytes_polyvec_i16(pf.s_bits[i]);
		}
	
		return out;
	}
	
	static size_t bytes_response_rsmall(const preauth::ResponseMsg& rsp) {
		return bytes_polyvec_i16(rsp.r_small);
	}
	
	static size_t bytes_response_C(const preauth::ResponseMsg& rsp) {
		return bytes_polyvec_i16(rsp.C);
	}
	
	static size_t bytes_response_z(const preauth::ResponseMsg& rsp) {
		return bytes_polyvec_i16(rsp.z);
	}
	
	static size_t bytes_response_meta(const preauth::ResponseMsg& rsp) {
		return bytes_repair_meta(rsp.meta);
	}
	
	static size_t bytes_response_proof(const preauth::ResponseMsg& rsp) {
		return bytes_repair_sigma_proof(rsp.proof);
	}
	
	static size_t bytes_response_msg(const preauth::ResponseMsg& rsp) {
		return bytes_response_rsmall(rsp)
			+ bytes_response_C(rsp)
			+ bytes_response_z(rsp)
			+ bytes_response_meta(rsp)
			+ bytes_response_proof(rsp);
	}
	
	static size_t bytes_auth_proof(const preauth::AuthProof& pf) {
		return bytes_commit_msg(pf.cm) + bytes_response_msg(pf.rsp);
	}

	static uint32_t exact_blob_version(const zk::RepairSigmaProof& pf) {
		if (pf.exact_proof_blob.size() < 4) return 0;
		return (uint32_t)pf.exact_proof_blob[0]
			| ((uint32_t)pf.exact_proof_blob[1] << 8)
			| ((uint32_t)pf.exact_proof_blob[2] << 16)
			| ((uint32_t)pf.exact_proof_blob[3] << 24);
	}

	static uint32_t read_u32_le(const std::vector<uint8_t>& v, size_t off) {
		if (off + 4 > v.size()) return 0;
		return (uint32_t)v[off]
			| ((uint32_t)v[off + 1] << 8)
			| ((uint32_t)v[off + 2] << 16)
			| ((uint32_t)v[off + 3] << 24);
	}

	static uint32_t exact_blob_header_bytes(const zk::RepairSigmaProof& pf) {
		const std::vector<uint8_t>& v = pf.exact_proof_blob;
		if (v.size() < 4) return 0;
		uint32_t ver = exact_blob_version(pf);
		if (ver == 3u) return 60u;
		if (ver == 4u) return 68u;
		if (ver == 5u) return 32u;
		if (ver == 6u) return 32u;
		if (ver == 7u) return 32u;
		return 0u;
	}

	static uint32_t exact_blob_payload_bytes(const zk::RepairSigmaProof& pf) {
		uint32_t hdr = exact_blob_header_bytes(pf);
		return (uint32_t)(pf.exact_proof_blob.size() >= hdr ? pf.exact_proof_blob.size() - hdr : 0u);
	}

} 

void run_exp_C1_overhead_and_size(const common::Params& base, int sessions) {
	ensure_dir_c1("data");
	const std::string ts = now_ymdhms_c1();
	const char* xof = "shake256";

	struct Case { int B; int eta; };
	std::vector<Case> cases;
	cases.push_back(Case{ 8, 2 });
	cases.push_back(Case{ 4, 4 });
	cases.push_back(Case{ 3, 6 });
	
	std::ostringstream fn;
	fn << "data/exp_C1_overhead_size_" << ts
		<< "_xof" << xof
		<< "_N" << base.N << "_k" << base.k << "_q" << base.q
		<< "_sessions" << sessions
		<< "_seed" << hex_noprefix_u64_c1(base.seed)
		<< ".csv";
	
	std::ofstream ofs(fn.str().c_str(), std::ios::out | std::ios::trunc);
	if (!ofs) {
		std::cout << "cannot open " << fn.str() << "\n";
		return;
	}
	
	ofs << "case_id,B,eta,session_idx,verify_ok,total_ms,prove_ms,verify_ms,"
		"commit_bytes,response_bytes,proof_bytes,authproof_bytes,"
		"rsmall_bytes,C_bytes,z_bytes,meta_bytes,proof_L,backend_kind,relation_kind,exact_mode,exact_blob_version,exact_blob_header_bytes,exact_blob_payload_bytes,exact_u_max,exact_boundary_count,exact_interior_count,relation_digest_bytes,exact_digest_bytes,exact_blob_bytes,audit_cnt,"
		"clamp_count,zstar_maxabs_centered,c_scalar\n";
	
	const uint64_t scope = 0x1111ULL;
	
	for (int ci = 0; ci < (int)cases.size(); ci++) {
		common::Params p = base;
		p.offload_proto = 0;
		p.B = cases[ci].B;
		p.eta = cases[ci].eta;
	
		common::Rng krng(p.seed ^ 0xC1000000ULL ^ (uint64_t)ci * 0x9E3779B97F4A7C15ULL);
		lattice::KeyPair kp = lattice::keygen(p, krng);
	
		common::Rng rng(p.seed ^ 0xC1C1C1C1ULL ^ (uint64_t)ci);
		preauth::Prover P(p, kp.pk, kp.sk);
		preauth::Verifier V(p, kp.pk);
		P.set_scope(scope);
		V.set_scope(scope);
	
		for (int si = 0; si < sessions; si++) {
			preauth::ProveObs obs;
	
			common::Timer tt;
			common::Timer tp;
			preauth::AuthProof pf = P.prove_nizk_observe(rng, &obs);
			double prove_ms = tp.ms();
	
			common::Timer tv;
			bool ok = V.verify_nizk(pf);
			double verify_ms = tv.ms();
			double total_ms = tt.ms();
	
			if (ok) P.on_accept(pf.cm, pf.rsp);
	
			const size_t commit_bytes = bytes_commit_msg(pf.cm);
			const size_t rsmall_bytes = bytes_response_rsmall(pf.rsp);
			const size_t C_bytes = bytes_response_C(pf.rsp);
			const size_t z_bytes = bytes_response_z(pf.rsp);
			const size_t meta_bytes = bytes_response_meta(pf.rsp);
			const size_t proof_bytes = bytes_response_proof(pf.rsp);
			const size_t response_bytes = bytes_response_msg(pf.rsp);
			const size_t authproof_bytes = bytes_auth_proof(pf);
	
			ofs << ci << "," << p.B << "," << p.eta << ","
				<< si << "," << (ok ? 1 : 0) << ","
				<< total_ms << "," << prove_ms << "," << verify_ms << ","
				<< (uint64_t)commit_bytes << ","
				<< (uint64_t)response_bytes << ","
				<< (uint64_t)proof_bytes << ","
				<< (uint64_t)authproof_bytes << ","
				<< (uint64_t)rsmall_bytes << ","
				<< (uint64_t)C_bytes << ","
				<< (uint64_t)z_bytes << ","
				<< (uint64_t)meta_bytes << ","
				<< pf.rsp.proof.L << ","
				<< pf.rsp.proof.backend_kind << ","
				<< pf.rsp.proof.relation_kind << ","
				<< pf.rsp.proof.exact_mode << ","
				<< exact_blob_version(pf.rsp.proof) << ","
				<< exact_blob_header_bytes(pf.rsp.proof) << ","
				<< exact_blob_payload_bytes(pf.rsp.proof) << ","
				<< pf.rsp.proof.exact_u_max << ","
				<< pf.rsp.proof.exact_boundary_count << ","
				<< pf.rsp.proof.exact_interior_count << ","
				<< (uint64_t)pf.rsp.proof.relation_digest.size() << ","
				<< (uint64_t)pf.rsp.proof.exact_digest.size() << ","
				<< (uint64_t)pf.rsp.proof.exact_proof_blob.size() << ","
				<< (uint64_t)obs.audit_cnt << ","
				<< obs.clamp_count << ","
				<< obs.zstar_maxabs_centered << ","
				<< obs.c_scalar << "\n";
		}
	}
	
	ofs.flush();
	std::cout << "[EXP] wrote: " << fn.str() << "\n";
}
