#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>

#include "common/params.h"
#include "common/rng.h"
#include "lattice/module_lwe.h"

#include "preauth/commit.h"
#include "preauth/repair.h"
#include "zk/repair_sigma.h"

namespace preauth {

	
	uint64_t DerivePseudonym(const common::Params& p, uint64_t scope_tag, const lattice::PolyVec& pk_t);

	
	
	std::vector<uint8_t> BuildRepairContextDigest(const common::Params& p,
		const lattice::PolyVec& member_t,
		const CommitMsg& cm,
		const std::vector<int>& c_bits);
	
	struct ResponseMsg {
		lattice::PolyVec r_small;  
		lattice::PolyVec C;        
		lattice::PolyVec z;        
		lattice::PolyVec delta;    
		RepairMeta meta;           
		zk::RepairSigmaProof proof;
	};
	
	struct AuthProof {
		CommitMsg cm;
		ResponseMsg rsp;
	};
	
	
	struct ProveObs {
		int zstar_maxabs_centered = 0; 
		int clamp_count = 0;           
		int delta_l0 = 0;              
		int delta_linf = 0;            
		int z_repaired_linf = 0;       
		size_t audit_cnt = 0;          
		int c_scalar = 0;              
	};
	
	class Prover {
	public:
		Prover(const common::Params& p, const lattice::PublicKey& pk, const lattice::SecretKey& sk);
	
		void set_scope(uint64_t scope_tag);
		uint64_t scope_tag() const { return scope_tag_; }
		uint64_t pseudonym() const { return pseudonym_; }
	
		CommitMsg commit(common::Rng& rng);
		ResponseMsg respond(const std::vector<int>& c_bits);
		void on_accept(const CommitMsg& cm, const ResponseMsg& rsp);
	
		AuthProof prove_nizk(common::Rng& rng);
		AuthProof prove_nizk_observe(common::Rng& rng, ProveObs* obs);
	
		
		uint32_t epoch() const { return epoch_; }
		size_t leak_accum() const { return leak_accum_; }
		uint64_t refresh_state() const { return refresh_state_; }
	
	private:
		ResponseMsg respond_impl(const std::vector<int>& c_bits, ProveObs* obs);
	
		common::Params p_;
		lattice::PublicKey pk_;
		lattice::SecretKey sk_;
	
		lattice::PolyVec y_;
		CommitMsg last_commit_;
	
		uint64_t scope_tag_ = 0;
		uint64_t pseudonym_ = 0;
	
		uint64_t refresh_state_ = 0;
		uint32_t epoch_ = 0;
		size_t leak_accum_ = 0;
	};
	
	class Verifier {
	public:
		Verifier(const common::Params& p, const lattice::PublicKey& pk);
	
		void set_scope(uint64_t scope_tag);
		uint64_t scope_tag() const { return scope_tag_; }
	
		
		uint64_t register_member(const lattice::PolyVec& t);
	
		void add_revoked(uint64_t pseudonym);
		bool is_revoked(uint64_t pseudonym) const;
	
		std::vector<int> derive_challenge(const CommitMsg& cm);
		bool verify(const CommitMsg& cm, const std::vector<int>& c_bits, const ResponseMsg& rsp);
		bool verify_nizk(const AuthProof& pf);
	
		
		bool has_member(uint64_t pseudonym) const;
		uint32_t member_epoch(uint64_t pseudonym) const;
		size_t member_leak_accum(uint64_t pseudonym) const;
		uint64_t member_refresh_state(uint64_t pseudonym) const;
	
	private:
		struct MemberState {
			lattice::PolyVec t;
			uint64_t refresh_state = 0;
			uint32_t epoch = 0;
			size_t leak_accum = 0;
		};
	
		common::Params p_;
		lattice::PolyMat A_;
		lattice::PolyVec default_t_;
	
		uint64_t scope_tag_ = 0;
	
		std::unordered_map<uint64_t, MemberState> members_;
		std::unordered_set<uint64_t> revoked_;
	};

} 
