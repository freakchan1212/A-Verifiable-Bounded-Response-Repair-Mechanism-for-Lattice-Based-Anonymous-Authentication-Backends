#pragma once

#include <cstdint>
#include <vector>

#include "common/params.h"
#include "lattice/module_lwe.h"
#include "lattice/polyvec.h"
#include "preauth/commit.h"
#include "preauth/repair.h"
#include "zk/repair_sigma.h"

namespace zk {

static const uint32_t PROOF_ENGINE_REPAIR_RELATION_FULL_V1 = 1u;
static const uint32_t PROOF_ENGINE_ZK_BACKEND_RELATION_V1 = 1u;

struct RepairRelationStatement {
    preauth::CommitMsg cm;
    lattice::PolyVec C;
    lattice::PolyVec r_small;
    lattice::PolyVec y1;
    lattice::PolyVec y3;
    lattice::PolyVec y4;
    lattice::PolyVec z_public_centered;
    
    
    
    lattice::PolyVec delta_public_centered;
    uint32_t L = 0;
    uint32_t exact_u_max = 0;
    uint32_t boundary_count = 0;
    uint32_t interior_count = 0;
};

struct RepairRelationWitness {
    lattice::PolyVec zstar_centered;
    lattice::PolyVec r_wit_centered;
    std::vector<lattice::PolyVec> bit_wit;
};

struct RepairRelationIR {
    uint32_t relation_kind = PROOF_ENGINE_REPAIR_RELATION_FULL_V1;
    RepairRelationStatement stmt;
    RepairRelationWitness wit;
};

RepairRelationIR BuildRepairRelationIR(const common::Params& p,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& C,
    const lattice::PolyVec& r_small,
    const lattice::PolyVec& y1,
    const lattice::PolyVec& y3,
    const lattice::PolyVec& y4,
    const lattice::PolyVec& z_public_centered,
    const lattice::PolyVec& zstar_centered,
    const lattice::PolyVec& r_wit_centered,
    const std::vector<lattice::PolyVec>& bit_wit,
    uint32_t Lbits,
    uint32_t exact_u_max,
    uint32_t boundary_count,
    uint32_t interior_count);

RepairRelationIR BuildRepairRelationIR(const common::Params& p,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& C,
    const lattice::PolyVec& r_small,
    const lattice::PolyVec& y1,
    const lattice::PolyVec& y3,
    const lattice::PolyVec& y4,
    const lattice::PolyVec& z_public_centered,
    const lattice::PolyVec& delta_public_centered,
    const lattice::PolyVec& zstar_centered,
    const lattice::PolyVec& r_wit_centered,
    const std::vector<lattice::PolyVec>& bit_wit,
    uint32_t Lbits,
    uint32_t exact_u_max,
    uint32_t boundary_count,
    uint32_t interior_count);

bool BuildExactBackendArtifacts(const common::Params& p,
    const lattice::PublicKey& pk,
    const preauth::RepairTheta& th,
    const RepairRelationIR& ir,
    uint32_t backend_kind,
    std::vector<uint8_t>* relation_digest,
    std::vector<uint8_t>* exact_digest,
    std::vector<uint8_t>* proof_blob);

bool VerifyExactBackendArtifacts(const common::Params& p,
    const lattice::PublicKey& pk,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& y_expected,
    const zk::RepairSigmaProof& pf,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& C,
    const lattice::PolyVec& r_small,
    const lattice::PolyVec& z_public_centered);

bool VerifyExactBackendArtifacts(const common::Params& p,
    const lattice::PublicKey& pk,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& y_expected,
    const zk::RepairSigmaProof& pf,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& C,
    const lattice::PolyVec& r_small,
    const lattice::PolyVec& z_public_centered,
    const lattice::PolyVec& delta_public_centered);

} 
