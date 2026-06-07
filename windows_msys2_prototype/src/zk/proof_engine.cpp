
#include "zk/proof_engine.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/bytes.h"
#include "common/hash.h"
#include "lattice/mod_arith.h"
#include "zk/lattice_commit.h"

namespace zk {
namespace {

static std::vector<uint8_t> serialize_polyvec_i16_centered(const common::Params& p,
    const lattice::PolyVec& v) {
    std::vector<uint8_t> out;
    out.reserve((size_t)p.k * (size_t)p.N * 2);
    for (int i = 0; i < p.k; i++) {
        for (int j = 0; j < p.N; j++) {
            int x = lattice::center_q(v.v[i].c[j], p.q);
            common::bytes::append_i16(out, (int16_t)x);
        }
    }
    return out;
}

static lattice::PolyVec derive_commit_witness_centered(const common::Params& p,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& z_public_centered) {
    std::vector<uint8_t> seedbuf;
    common::bytes::append_str(seedbuf, "ComRand-stage3");
    common::bytes::append_u64(seedbuf, p.seed);
    common::bytes::append_u32(seedbuf, cm.nonce);
    common::bytes::append_u64(seedbuf, cm.refresh_tag);
    common::bytes::append_u64(seedbuf, cm.scope_tag);
    common::bytes::append_u64(seedbuf, cm.pseudonym);
    common::bytes::append_bytes(seedbuf, serialize_polyvec_i16_centered(p, z_public_centered));
    uint64_t r_seed = common::hash::hash64(seedbuf);
    common::Rng r_rng(r_seed);
    lattice::PolyVec r_wit = lattice::vec_uniform(p, r_rng);
    return lattice::vec_center_reduce(p, r_wit);
}

static bool reconstruct_zstar_from_bits(const common::Params& p,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& z_public_centered,
    const std::vector<lattice::PolyVec>& bits,
    uint32_t Lbits,
    lattice::PolyVec& zstar_centered) {
    if (bits.size() != Lbits) return false;
    zstar_centered = lattice::vec_zero(p);
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j) {
            int zc = z_public_centered.v[i].c[j];
            bool is_boundary = (zc == th.B || zc == -th.B);
            if (!is_boundary) {
                zstar_centered.v[i].c[j] = zc;
                continue;
            }
            int u = 0;
            for (uint32_t b = 0; b < Lbits; ++b) {
                int bit = lattice::center_q(bits[b].v[i].c[j], p.q);
                if (bit != 0 && bit != 1) return false;
                if (bit == 1) u += (1 << b);
            }
            zstar_centered.v[i].c[j] = (zc == th.B) ? (th.B + u) : (-th.B - u);
        }
    }
    return true;
}

static lattice::PolyVec to_modq(const common::Params& p,
    const lattice::PolyVec& v_centered) {
    lattice::PolyVec out = v_centered;
    for (int i = 0; i < p.k; i++) {
        for (int j = 0; j < p.N; j++) {
            out.v[i].c[j] = lattice::modq(out.v[i].c[j], p.q);
        }
    }
    return out;
}

static lattice::PolyVec apply_mask01_modq(const common::Params& p,
    const lattice::PolyVec& v_modq,
    const std::vector<uint8_t>& mask01) {
    lattice::PolyVec out = lattice::vec_zero(p);
    int idx = 0;
    for (int i = 0; i < p.k; i++) {
        for (int j = 0; j < p.N; j++, idx++) {
            int m = (idx < (int)mask01.size()) ? (mask01[idx] & 1) : 0;
            out.v[i].c[j] = lattice::modq(m * lattice::modq(v_modq.v[i].c[j], p.q), p.q);
        }
    }
    return out;
}

static bool polyvec_eq_modq(const common::Params& p,
    const lattice::PolyVec& a,
    const lattice::PolyVec& b) {
    if ((int)a.v.size() != p.k || (int)b.v.size() != p.k) return false;
    for (int i = 0; i < p.k; i++) {
        if ((int)a.v[i].c.size() != p.N || (int)b.v[i].c.size() != p.N) return false;
        for (int j = 0; j < p.N; j++) {
            if (lattice::modq(a.v[i].c[j], p.q) != lattice::modq(b.v[i].c[j], p.q)) return false;
        }
    }
    return true;
}

static bool polyvec_shape_ok(const common::Params& p, const lattice::PolyVec& v) {
    if ((int)v.v.size() != p.k) return false;
    for (int i = 0; i < p.k; ++i) {
        if ((int)v.v[(size_t)i].c.size() != p.N) return false;
    }
    return true;
}

static lattice::PolyVec derive_delta_centered_from_zstar(const common::Params& p,
    const lattice::PolyVec& z_public_centered,
    const lattice::PolyVec& zstar_centered) {
    lattice::PolyVec delta = lattice::vec_zero(p);
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j) {
            const int zc = lattice::center_q((int)z_public_centered.v[(size_t)i].c[(size_t)j], p.q);
            const int xs = lattice::center_q((int)zstar_centered.v[(size_t)i].c[(size_t)j], p.q);
            delta.v[(size_t)i].c[(size_t)j] = (lattice::Poly::coeff_t)lattice::center_q(xs - zc, p.q);
        }
    }
    return delta;
}

static bool delta_reconstructs_zstar_centered(const common::Params& p,
    const lattice::PolyVec& z_public_centered,
    const lattice::PolyVec& delta_public_centered,
    const lattice::PolyVec& zstar_centered) {
    if (!polyvec_shape_ok(p, z_public_centered)) return false;
    if (!polyvec_shape_ok(p, delta_public_centered)) return false;
    if (!polyvec_shape_ok(p, zstar_centered)) return false;
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j) {
            const int zc = lattice::center_q((int)z_public_centered.v[(size_t)i].c[(size_t)j], p.q);
            const int dc = lattice::center_q((int)delta_public_centered.v[(size_t)i].c[(size_t)j], p.q);
            const int xs = lattice::center_q((int)zstar_centered.v[(size_t)i].c[(size_t)j], p.q);
            if (lattice::center_q(zc + dc, p.q) != xs) return false;
        }
    }
    return true;
}

static bool delta_matches_saturating_repair_rule(const common::Params& p,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& z_public_centered,
    const lattice::PolyVec& delta_public_centered,
    const lattice::PolyVec& zstar_centered) {
    if (!delta_reconstructs_zstar_centered(p, z_public_centered, delta_public_centered, zstar_centered)) return false;
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j) {
            const int raw = lattice::center_q((int)zstar_centered.v[(size_t)i].c[(size_t)j], p.q);
            int expected_z = raw;
            if (raw > th.B) expected_z = th.B;
            else if (raw < -th.B) expected_z = -th.B;
            const int expected_delta = raw - expected_z;
            const int zc = lattice::center_q((int)z_public_centered.v[(size_t)i].c[(size_t)j], p.q);
            const int dc = lattice::center_q((int)delta_public_centered.v[(size_t)i].c[(size_t)j], p.q);
            if (zc != expected_z) return false;
            if (dc != expected_delta) return false;
        }
    }
    return true;
}

static const lattice::PolyMat& get_cached_commit_key(const common::Params& p,
    const lattice::PublicKey& pk) {
    static thread_local lattice::PolyMat cached;
    static thread_local const lattice::PublicKey* last_pk = nullptr;
    static thread_local int last_N = -1;
    static thread_local int last_q = -1;
    static thread_local int last_k = -1;
    static thread_local uint64_t last_seed = 0;
    if (last_pk != &pk || last_N != p.N || last_q != p.q || last_k != p.k || last_seed != p.seed) {
        cached = DeriveCommitKey(p, pk);
        last_pk = &pk;
        last_N = p.N;
        last_q = p.q;
        last_k = p.k;
        last_seed = p.seed;
    }
    return cached;
}

static void fill_public_masked_views(const common::Params& p,
    const std::vector<uint8_t>& D_boundary,
    const std::vector<uint8_t>& M_interior,
    const std::vector<int>& off_centered,
    const lattice::PolyVec& z_public_centered,
    lattice::PolyVec& y3,
    lattice::PolyVec& y4) {
    y3 = lattice::vec_zero(p);
    y4 = lattice::vec_zero(p);
    int idx = 0;
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j, ++idx) {
            if (D_boundary[(size_t)idx]) {
                y3.v[i].c[j] = lattice::modq(off_centered[(size_t)idx], p.q);
            } else if (M_interior[(size_t)idx]) {
                y4.v[i].c[j] = lattice::modq(z_public_centered.v[i].c[j], p.q);
            }
        }
    }
}

static void build_masks(const common::Params& p,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& z_centered,
    std::vector<uint8_t>& D_boundary,
    std::vector<uint8_t>& M_interior,
    std::vector<int>& off_centered,
    std::vector<int>& sign) {
    const int T = p.k * p.N;
    D_boundary.assign(T, 0);
    M_interior.assign(T, 0);
    off_centered.assign(T, 0);
    sign.assign(T, 0);

    int idx = 0;
    for (int i = 0; i < p.k; i++) {
        for (int j = 0; j < p.N; j++, idx++) {
            int x = z_centered.v[i].c[j];
            if (x == th.B) {
                D_boundary[idx] = 1;
                off_centered[idx] = th.B;
                sign[idx] = +1;
            } else if (x == -th.B) {
                D_boundary[idx] = 1;
                off_centered[idx] = -th.B;
                sign[idx] = -1;
            } else if (x > -th.B && x < th.B) {
                M_interior[idx] = 1;
            }
        }
    }
}

static int max_boundary_overflow(const common::Params& p,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& z_centered,
    const lattice::PolyVec& zstar_centered) {
    int max_u = 0;
    for (int i = 0; i < p.k; i++) {
        for (int j = 0; j < p.N; j++) {
            int zc = z_centered.v[i].c[j];
            if (zc != th.B && zc != -th.B) continue;
            int xs = lattice::center_q(zstar_centered.v[i].c[j], p.q);
            int u = (zc == th.B) ? (xs - th.B) : ((-th.B) - xs);
            if (u < 0) u = 0;
            if (u > max_u) max_u = u;
        }
    }
    return max_u;
}

static bool validate_bit_planes_exact(const common::Params& p,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& z_centered,
    const lattice::PolyVec& zstar_centered,
    const std::vector<lattice::PolyVec>& bits,
    uint32_t Lbits) {
    if (bits.size() != Lbits) return false;
    for (uint32_t b = 0; b < Lbits; b++) {
        if ((int)bits[b].v.size() != p.k) return false;
        for (int i = 0; i < p.k; i++) {
            if ((int)bits[b].v[i].c.size() != p.N) return false;
        }
    }

    for (int i = 0; i < p.k; i++) {
        for (int j = 0; j < p.N; j++) {
            const int zc = z_centered.v[i].c[j];
            const int xs = lattice::center_q(zstar_centered.v[i].c[j], p.q);
            const bool is_boundary = (zc == th.B || zc == -th.B);
            const bool is_interior = (zc > -th.B && zc < th.B);
            if (!is_boundary && !is_interior) return false;

            int64_t recon_u = 0;
            for (uint32_t b = 0; b < Lbits; b++) {
                const int bit = lattice::center_q(bits[b].v[i].c[j], p.q);
                if (bit != 0 && bit != 1) return false;
                if (!is_boundary && bit != 0) return false;
                if (bit == 1) recon_u += (1LL << b);
            }

            if (is_interior) {
                if (xs != zc) return false;
                continue;
            }

            int expected_u = 0;
            if (zc == th.B) {
                if (xs < th.B) return false;
                expected_u = xs - th.B;
            } else {
                if (xs > -th.B) return false;
                expected_u = (-th.B) - xs;
            }
            if (expected_u < 0) return false;
            if (recon_u != (int64_t)expected_u) return false;
        }
    }
    return true;
}

static std::vector<uint8_t> derive_relation_statement_digest(const common::Params& p,
    const RepairRelationStatement& st) {
    std::vector<uint8_t> buf;
    common::bytes::append_str(buf, "RepairRelationStmt-v2-delta");
    common::bytes::append_u64(buf, p.seed);
    common::bytes::append_u32(buf, st.cm.nonce);
    common::bytes::append_u64(buf, st.cm.scope_tag);
    common::bytes::append_u64(buf, st.cm.pseudonym);
    common::bytes::append_u64(buf, st.cm.refresh_tag);
    common::bytes::append_u32(buf, st.L);
    common::bytes::append_u32(buf, st.exact_u_max);
    common::bytes::append_u32(buf, st.boundary_count);
    common::bytes::append_u32(buf, st.interior_count);
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, st.cm.w));
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, st.C));
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, st.r_small));
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, st.y1));
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, st.y3));
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, st.y4));
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, st.z_public_centered));
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, st.delta_public_centered));
    return common::hash::hash32(buf);
}

static std::vector<uint8_t> derive_relation_digest(const common::Params& p,
    const RepairRelationIR& ir,
    const std::vector<uint8_t>& stmt_digest) {
    std::vector<uint8_t> buf;
    common::bytes::append_str(buf, "RepairRelationDigest-v1");
    common::bytes::append_u64(buf, p.seed);
    common::bytes::append_u32(buf, ir.relation_kind);
    common::bytes::append_bytes(buf, stmt_digest);
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, ir.wit.zstar_centered));
    common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, ir.wit.r_wit_centered));
    for (uint32_t b = 0; b < ir.stmt.L; b++) {
        common::bytes::append_bytes(buf, serialize_polyvec_i16_centered(p, ir.wit.bit_wit[b]));
    }
    return common::hash::hash32(buf);
}

static std::vector<uint8_t> derive_relation_digest_v6(const common::Params& p,
    uint32_t relation_kind,
    const std::vector<uint8_t>& stmt_digest,
    const std::vector<uint8_t>& packed_boundary_bits) {
    std::vector<uint8_t> buf;
    common::bytes::append_str(buf, "RepairRelationDigest-v6");
    common::bytes::append_u64(buf, p.seed);
    common::bytes::append_u32(buf, relation_kind);
    common::bytes::append_bytes(buf, stmt_digest);
    common::bytes::append_u32(buf, (uint32_t)packed_boundary_bits.size());
    common::bytes::append_bytes(buf, packed_boundary_bits);
    return common::hash::hash32(buf);
}

static std::vector<uint8_t> derive_relation_digest_v7(const common::Params& p,
    uint32_t relation_kind,
    const std::vector<uint8_t>& stmt_digest,
    const std::vector<uint8_t>& packed_boundary_bits) {
    std::vector<uint8_t> buf;
    common::bytes::append_str(buf, "RepairRelationDigest-v7");
    common::bytes::append_u64(buf, p.seed);
    common::bytes::append_u32(buf, relation_kind);
    common::bytes::append_bytes(buf, stmt_digest);
    common::bytes::append_u32(buf, (uint32_t)packed_boundary_bits.size());
    common::bytes::append_bytes(buf, packed_boundary_bits);
    return common::hash::hash32(buf);
}

static size_t packed_bits_len(size_t nbits) {
    return (nbits + 7u) / 8u;
}

static std::vector<uint8_t> pack_mask_bits(const std::vector<uint8_t>& bits01) {
    std::vector<uint8_t> out(packed_bits_len(bits01.size()), 0);
    for (size_t i = 0; i < bits01.size(); ++i) {
        if (bits01[i] & 1u) out[i >> 3] |= (uint8_t)(1u << (i & 7u));
    }
    return out;
}

static bool unpack_mask_bits(const std::vector<uint8_t>& packed,
    size_t nbits,
    std::vector<uint8_t>& bits01) {
    if (packed.size() != packed_bits_len(nbits)) return false;
    bits01.assign(nbits, 0);
    for (size_t i = 0; i < nbits; ++i) {
        bits01[i] = (uint8_t)((packed[i >> 3] >> (i & 7u)) & 1u);
    }
    return true;
}

static std::vector<uint8_t> pack_boundary_plane_bits(const common::Params& p,
    const std::vector<uint8_t>& D_boundary,
    const std::vector<lattice::PolyVec>& bit_wit,
    uint32_t L) {
    const size_t boundary_count = [&]() {
        size_t c = 0;
        for (size_t i = 0; i < D_boundary.size(); ++i) if (D_boundary[i]) ++c;
        return c;
    }();
    std::vector<uint8_t> out(packed_bits_len(boundary_count * (size_t)L), 0);
    size_t bitpos = 0;
    int idx = 0;
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j, ++idx) {
            if (!D_boundary[(size_t)idx]) continue;
            for (uint32_t b = 0; b < L; ++b, ++bitpos) {
                int val = lattice::center_q(bit_wit[b].v[i].c[j], p.q);
                if (val & 1) out[bitpos >> 3] |= (uint8_t)(1u << (bitpos & 7u));
            }
        }
    }
    return out;
}

static bool unpack_boundary_plane_bits(const common::Params& p,
    const std::vector<uint8_t>& D_boundary,
    const std::vector<uint8_t>& packed,
    uint32_t L,
    std::vector<lattice::PolyVec>& bit_wit) {
    size_t boundary_count = 0;
    for (size_t i = 0; i < D_boundary.size(); ++i) if (D_boundary[i]) ++boundary_count;
    if (packed.size() != packed_bits_len(boundary_count * (size_t)L)) return false;
    bit_wit.assign(L, lattice::vec_zero(p));
    size_t bitpos = 0;
    int idx = 0;
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j, ++idx) {
            if (!D_boundary[(size_t)idx]) continue;
            for (uint32_t b = 0; b < L; ++b, ++bitpos) {
                int val = (packed[bitpos >> 3] >> (bitpos & 7u)) & 1u;
                bit_wit[b].v[i].c[j] = val;
            }
        }
    }
    return true;
}

static bool reconstruct_zstar_from_packed_boundary_bits(const common::Params& p,
    const preauth::RepairTheta& th,
    const std::vector<uint8_t>& D_boundary,
    const lattice::PolyVec& z_public_centered,
    const std::vector<uint8_t>& packed_boundary_bits,
    uint32_t L,
    lattice::PolyVec& zstar_centered,
    uint32_t* max_u_out = nullptr) {
    size_t boundary_count = 0;
    for (size_t i = 0; i < D_boundary.size(); ++i) if (D_boundary[i]) ++boundary_count;
    if (packed_boundary_bits.size() != packed_bits_len(boundary_count * (size_t)L)) return false;
    zstar_centered = lattice::vec_zero(p);
    size_t bitpos = 0;
    uint32_t max_u = 0;
    int idx = 0;
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j, ++idx) {
            int zc = z_public_centered.v[i].c[j];
            if (!D_boundary[(size_t)idx]) {
                zstar_centered.v[i].c[j] = zc;
                continue;
            }
            if (zc != th.B && zc != -th.B) return false;
            uint32_t u = 0;
            for (uint32_t b = 0; b < L; ++b, ++bitpos) {
                uint32_t bit = (uint32_t)((packed_boundary_bits[bitpos >> 3] >> (bitpos & 7u)) & 1u);
                if (bit) u |= (uint32_t(1) << b);
            }
            if (u > max_u) max_u = u;
            zstar_centered.v[i].c[j] = (zc == th.B) ? (th.B + (int)u) : (-th.B - (int)u);
        }
    }
    if (bitpos != boundary_count * (size_t)L) return false;
    if (max_u_out) *max_u_out = max_u;
    return true;
}

static std::vector<uint8_t> build_relation_v1_blob(const common::Params& p,
    const RepairRelationIR& ir,
    const std::vector<uint8_t>& stmt_digest) {
    std::vector<uint8_t> blob;
    common::bytes::append_u32(blob, 3u); 
    common::bytes::append_u32(blob, (uint32_t)PROOF_ENGINE_ZK_BACKEND_RELATION_V1);
    common::bytes::append_u32(blob, ir.relation_kind);
    common::bytes::append_u32(blob, ir.stmt.L);
    common::bytes::append_u32(blob, ir.stmt.exact_u_max);
    common::bytes::append_u32(blob, ir.stmt.boundary_count);
    common::bytes::append_u32(blob, ir.stmt.interior_count);
    common::bytes::append_bytes(blob, stmt_digest);
    common::bytes::append_bytes(blob, serialize_polyvec_i16_centered(p, ir.wit.zstar_centered));
    common::bytes::append_bytes(blob, serialize_polyvec_i16_centered(p, ir.wit.r_wit_centered));
    for (uint32_t b = 0; b < ir.stmt.L; b++) {
        common::bytes::append_bytes(blob, serialize_polyvec_i16_centered(p, ir.wit.bit_wit[b]));
    }
    return blob;
}

static std::vector<uint8_t> build_relation_v6_minimal_blob(const common::Params& p,
    const RepairRelationIR& ir) {
    std::vector<uint8_t> D_boundary;
    const int T = p.k * p.N;
    D_boundary.assign((size_t)T, 0);
    int idx = 0;
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j, ++idx) {
            D_boundary[(size_t)idx] = (lattice::modq(ir.stmt.y3.v[i].c[j], p.q) != 0) ? 1u : 0u;
        }
    }

    std::vector<uint8_t> packed_boundary_bits = pack_boundary_plane_bits(p, D_boundary, ir.wit.bit_wit, ir.stmt.L);

    std::vector<uint8_t> blob;
    common::bytes::append_u32(blob, 6u); 
    common::bytes::append_u32(blob, (uint32_t)PROOF_ENGINE_ZK_BACKEND_RELATION_V1);
    common::bytes::append_u32(blob, ir.relation_kind);
    common::bytes::append_u32(blob, ir.stmt.L);
    common::bytes::append_u32(blob, ir.stmt.exact_u_max);
    common::bytes::append_u32(blob, ir.stmt.boundary_count);
    common::bytes::append_u32(blob, ir.stmt.interior_count);
    common::bytes::append_u32(blob, (uint32_t)packed_boundary_bits.size());
    common::bytes::append_bytes(blob, packed_boundary_bits);
    return blob;
}

static std::vector<uint8_t> build_relation_v7_minimal_blob(const common::Params& p,
    const RepairRelationIR& ir) {
    std::vector<uint8_t> D_boundary;
    const int T = p.k * p.N;
    D_boundary.assign((size_t)T, 0);
    int idx = 0;
    for (int i = 0; i < p.k; ++i) {
        for (int j = 0; j < p.N; ++j, ++idx) {
            D_boundary[(size_t)idx] = (lattice::modq(ir.stmt.y3.v[i].c[j], p.q) != 0) ? 1u : 0u;
        }
    }

    std::vector<uint8_t> packed_boundary_bits = pack_boundary_plane_bits(p, D_boundary, ir.wit.bit_wit, ir.stmt.L);

    std::vector<uint8_t> blob;
    common::bytes::append_u32(blob, 7u); 
    common::bytes::append_u32(blob, (uint32_t)PROOF_ENGINE_ZK_BACKEND_RELATION_V1);
    common::bytes::append_u32(blob, ir.relation_kind);
    common::bytes::append_u32(blob, ir.stmt.L);
    common::bytes::append_u32(blob, ir.stmt.exact_u_max);
    common::bytes::append_u32(blob, ir.stmt.boundary_count);
    common::bytes::append_u32(blob, ir.stmt.interior_count);
    common::bytes::append_u32(blob, (uint32_t)packed_boundary_bits.size());
    common::bytes::append_bytes(blob, packed_boundary_bits);
    return blob;
}

static bool read_u32_blob(const std::vector<uint8_t>& blob, size_t& pos, uint32_t& out) {
    if (pos + 4 > blob.size()) return false;
    out = (uint32_t)blob[pos]
        | ((uint32_t)blob[pos + 1] << 8)
        | ((uint32_t)blob[pos + 2] << 16)
        | ((uint32_t)blob[pos + 3] << 24);
    pos += 4;
    return true;
}

static bool read_bytes_blob(const std::vector<uint8_t>& blob, size_t& pos, size_t len, std::vector<uint8_t>& out) {
    if (pos + len > blob.size()) return false;
    out.assign(blob.begin() + (std::ptrdiff_t)pos, blob.begin() + (std::ptrdiff_t)(pos + len));
    pos += len;
    return true;
}

static bool read_polyvec_i16_blob(const common::Params& p,
    const std::vector<uint8_t>& blob,
    size_t& pos,
    lattice::PolyVec& out) {
    out = lattice::vec_zero(p);
    for (int i = 0; i < p.k; i++) {
        for (int j = 0; j < p.N; j++) {
            if (pos + 2 > blob.size()) return false;
            uint16_t lo = (uint16_t)blob[pos];
            uint16_t hi = (uint16_t)blob[pos + 1];
            int16_t x = (int16_t)(lo | (hi << 8));
            out.v[i].c[j] = (int)x;
            pos += 2;
        }
    }
    return true;
}

static bool decode_relation_v1_blob(const common::Params& p,
    const std::vector<uint8_t>& blob,
    RepairRelationIR& ir,
    std::vector<uint8_t>& stmt_digest,
    uint32_t& backend_kind) {
    size_t pos = 0;
    uint32_t version = 0;
    if (!read_u32_blob(blob, pos, version)) return false;
    if (!read_u32_blob(blob, pos, backend_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.relation_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.L)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.exact_u_max)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.boundary_count)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.interior_count)) return false;
    if (version != 3u) return false;
    if (backend_kind != (uint32_t)PROOF_ENGINE_ZK_BACKEND_RELATION_V1) return false;
    if (!read_bytes_blob(blob, pos, 32, stmt_digest)) return false;
    if (!read_polyvec_i16_blob(p, blob, pos, ir.wit.zstar_centered)) return false;
    if (!read_polyvec_i16_blob(p, blob, pos, ir.wit.r_wit_centered)) return false;
    ir.wit.bit_wit.assign(ir.stmt.L, lattice::vec_zero(p));
    for (uint32_t b = 0; b < ir.stmt.L; b++) {
        if (!read_polyvec_i16_blob(p, blob, pos, ir.wit.bit_wit[b])) return false;
    }
    return pos == blob.size();
}

static bool decode_relation_v2_compact_blob(const common::Params& p,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& z_public_centered,
    const std::vector<uint8_t>& blob,
    RepairRelationIR& ir,
    std::vector<uint8_t>& stmt_digest,
    uint32_t& backend_kind) {
    size_t pos = 0;
    uint32_t version = 0;
    uint32_t packed_mask_bytes = 0;
    uint32_t packed_boundary_bits_bytes = 0;
    std::vector<uint8_t> packed_mask;
    std::vector<uint8_t> packed_boundary_bits;
    if (!read_u32_blob(blob, pos, version)) return false;
    if (!read_u32_blob(blob, pos, backend_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.relation_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.L)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.exact_u_max)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.boundary_count)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.interior_count)) return false;
    if (version != 4u) return false;
    if (backend_kind != (uint32_t)PROOF_ENGINE_ZK_BACKEND_RELATION_V1) return false;
    if (!read_bytes_blob(blob, pos, 32, stmt_digest)) return false;
    if (!read_u32_blob(blob, pos, packed_mask_bytes)) return false;
    if (!read_u32_blob(blob, pos, packed_boundary_bits_bytes)) return false;
    if (!read_bytes_blob(blob, pos, packed_mask_bytes, packed_mask)) return false;
    if (!read_polyvec_i16_blob(p, blob, pos, ir.wit.zstar_centered)) return false;
    if (!read_polyvec_i16_blob(p, blob, pos, ir.wit.r_wit_centered)) return false;
    if (!read_bytes_blob(blob, pos, packed_boundary_bits_bytes, packed_boundary_bits)) return false;
    if (pos != blob.size()) return false;

    const size_t T = (size_t)p.k * (size_t)p.N;
    std::vector<uint8_t> D_from_blob;
    if (!unpack_mask_bits(packed_mask, T, D_from_blob)) return false;

    std::vector<uint8_t> D_public, M_public;
    std::vector<int> off_public, sign_public;
    build_masks(p, th, z_public_centered, D_public, M_public, off_public, sign_public);
    if (D_from_blob != D_public) return false;

    size_t boundary_count = 0;
    for (size_t i = 0; i < D_public.size(); ++i) if (D_public[i]) ++boundary_count;
    if ((uint32_t)boundary_count != ir.stmt.boundary_count) return false;
    if ((uint32_t)(T - boundary_count) != ir.stmt.interior_count) return false;

    if (!unpack_boundary_plane_bits(p, D_public, packed_boundary_bits, ir.stmt.L, ir.wit.bit_wit)) return false;
    return true;
}

static bool decode_relation_v5_minimal_blob(const common::Params& p,
    const preauth::RepairTheta& th,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& z_public_centered,
    const std::vector<uint8_t>& blob,
    RepairRelationIR& ir,
    uint32_t& backend_kind) {
    size_t pos = 0;
    uint32_t version = 0;
    uint32_t packed_boundary_bits_bytes = 0;
    std::vector<uint8_t> packed_boundary_bits;
    if (!read_u32_blob(blob, pos, version)) return false;
    if (!read_u32_blob(blob, pos, backend_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.relation_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.L)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.exact_u_max)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.boundary_count)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.interior_count)) return false;
    if (!read_u32_blob(blob, pos, packed_boundary_bits_bytes)) return false;
    if (version != 5u) return false;
    if (backend_kind != (uint32_t)PROOF_ENGINE_ZK_BACKEND_RELATION_V1) return false;
    if (!read_bytes_blob(blob, pos, packed_boundary_bits_bytes, packed_boundary_bits)) return false;
    if (pos != blob.size()) return false;

    std::vector<uint8_t> D_public, M_public;
    std::vector<int> off_public, sign_public;
    build_masks(p, th, z_public_centered, D_public, M_public, off_public, sign_public);

    size_t boundary_count = 0;
    for (size_t i = 0; i < D_public.size(); ++i) if (D_public[i]) ++boundary_count;
    if ((uint32_t)boundary_count != ir.stmt.boundary_count) return false;
    if ((uint32_t)(((size_t)p.k * (size_t)p.N) - boundary_count) != ir.stmt.interior_count) return false;

    if (!unpack_boundary_plane_bits(p, D_public, packed_boundary_bits, ir.stmt.L, ir.wit.bit_wit)) return false;
    if (!reconstruct_zstar_from_bits(p, th, z_public_centered, ir.wit.bit_wit, ir.stmt.L, ir.wit.zstar_centered)) return false;
    ir.wit.r_wit_centered = derive_commit_witness_centered(p, cm, z_public_centered);
    return true;
}

static bool decode_relation_v6_minimal_blob(const common::Params& p,
    const preauth::RepairTheta& th,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& z_public_centered,
    const std::vector<uint8_t>& blob,
    RepairRelationIR& ir,
    uint32_t& backend_kind,
    std::vector<uint8_t>& packed_boundary_bits) {
    size_t pos = 0;
    uint32_t version = 0;
    uint32_t packed_boundary_bits_bytes = 0;
    if (!read_u32_blob(blob, pos, version)) return false;
    if (!read_u32_blob(blob, pos, backend_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.relation_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.L)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.exact_u_max)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.boundary_count)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.interior_count)) return false;
    if (!read_u32_blob(blob, pos, packed_boundary_bits_bytes)) return false;
    if (version != 6u) return false;
    if (backend_kind != (uint32_t)PROOF_ENGINE_ZK_BACKEND_RELATION_V1) return false;
    if (!read_bytes_blob(blob, pos, packed_boundary_bits_bytes, packed_boundary_bits)) return false;
    if (pos != blob.size()) return false;

    std::vector<uint8_t> D_public, M_public;
    std::vector<int> off_public, sign_public;
    build_masks(p, th, z_public_centered, D_public, M_public, off_public, sign_public);

    size_t boundary_count = 0;
    for (size_t i = 0; i < D_public.size(); ++i) if (D_public[i]) ++boundary_count;
    if ((uint32_t)boundary_count != ir.stmt.boundary_count) return false;
    if ((uint32_t)(((size_t)p.k * (size_t)p.N) - boundary_count) != ir.stmt.interior_count) return false;

    if (boundary_count == 0) {
        if (!packed_boundary_bits.empty()) return false;
        ir.wit.zstar_centered = z_public_centered;
    } else {
        if (!reconstruct_zstar_from_packed_boundary_bits(p, th, D_public, z_public_centered,
                packed_boundary_bits, ir.stmt.L, ir.wit.zstar_centered)) return false;
    }
    ir.wit.r_wit_centered = derive_commit_witness_centered(p, cm, z_public_centered);
    return true;
}

static bool decode_relation_v7_minimal_blob(const common::Params& p,
    const preauth::RepairTheta& th,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& z_public_centered,
    const std::vector<uint8_t>& blob,
    RepairRelationIR& ir,
    uint32_t& backend_kind,
    std::vector<uint8_t>& packed_boundary_bits,
    std::vector<uint8_t>& D_public,
    std::vector<uint8_t>& M_public,
    std::vector<int>& off_public,
    uint32_t& max_u_from_bits) {
    size_t pos = 0;
    uint32_t version = 0;
    uint32_t packed_boundary_bits_bytes = 0;
    if (!read_u32_blob(blob, pos, version)) return false;
    if (!read_u32_blob(blob, pos, backend_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.relation_kind)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.L)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.exact_u_max)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.boundary_count)) return false;
    if (!read_u32_blob(blob, pos, ir.stmt.interior_count)) return false;
    if (!read_u32_blob(blob, pos, packed_boundary_bits_bytes)) return false;
    if (version != 7u) return false;
    if (backend_kind != (uint32_t)PROOF_ENGINE_ZK_BACKEND_RELATION_V1) return false;
    if (!read_bytes_blob(blob, pos, packed_boundary_bits_bytes, packed_boundary_bits)) return false;
    if (pos != blob.size()) return false;

    std::vector<int> sign_public;
    build_masks(p, th, z_public_centered, D_public, M_public, off_public, sign_public);

    size_t boundary_count = 0;
    for (size_t i = 0; i < D_public.size(); ++i) if (D_public[i]) ++boundary_count;
    if ((uint32_t)boundary_count != ir.stmt.boundary_count) return false;
    if ((uint32_t)(((size_t)p.k * (size_t)p.N) - boundary_count) != ir.stmt.interior_count) return false;

    max_u_from_bits = 0;
    if (boundary_count == 0) {
        if (!packed_boundary_bits.empty()) return false;
        ir.wit.zstar_centered = z_public_centered;
    } else {
        if (!reconstruct_zstar_from_packed_boundary_bits(p, th, D_public, z_public_centered,
                packed_boundary_bits, ir.stmt.L, ir.wit.zstar_centered, &max_u_from_bits)) return false;
    }
    ir.wit.r_wit_centered = derive_commit_witness_centered(p, cm, z_public_centered);
    return true;
}

static bool compute_exact_backend_outputs(const common::Params& p,
    const lattice::PublicKey& pk,
    const preauth::RepairTheta& th,
    const RepairRelationIR& ir,
    uint32_t backend_kind,
    std::vector<uint8_t>* relation_digest,
    std::vector<uint8_t>* exact_digest,
    std::vector<uint8_t>* proof_blob,
    bool emit_proof_blob) {
    if (!relation_digest || !exact_digest) return false;
    if (backend_kind != (uint32_t)PROOF_ENGINE_ZK_BACKEND_RELATION_V1) return false;
    if (ir.stmt.L == 0 || ir.wit.bit_wit.size() != ir.stmt.L) return false;

    if (!validate_bit_planes_exact(p, th, ir.stmt.z_public_centered, ir.wit.zstar_centered, ir.wit.bit_wit, ir.stmt.L)) return false;
    if ((uint32_t)max_boundary_overflow(p, th, ir.stmt.z_public_centered, ir.wit.zstar_centered) != ir.stmt.exact_u_max) return false;
    if (!delta_matches_saturating_repair_rule(p, th, ir.stmt.z_public_centered,
            ir.stmt.delta_public_centered, ir.wit.zstar_centered)) return false;

    std::vector<uint8_t> D, M;
    std::vector<int> off_centered, sign;
    build_masks(p, th, ir.stmt.z_public_centered, D, M, off_centered, sign);

    uint32_t actual_boundary_count = 0;
    uint32_t actual_interior_count = 0;
    int idx = 0;
    for (int i = 0; i < p.k; i++) {
        for (int j = 0; j < p.N; j++, idx++) {
            const int zc = ir.stmt.z_public_centered.v[i].c[j];
            if (zc == th.B || zc == -th.B) actual_boundary_count++;
            else if (zc > -th.B && zc < th.B) actual_interior_count++;
            else return false;
        }
    }
    if (actual_boundary_count != ir.stmt.boundary_count) return false;
    if (actual_interior_count != ir.stmt.interior_count) return false;

    lattice::PolyVec y3, y4;
    fill_public_masked_views(p, D, M, off_centered, ir.stmt.z_public_centered, y3, y4);
    if (!polyvec_eq_modq(p, y3, ir.stmt.y3)) return false;
    if (!polyvec_eq_modq(p, y4, ir.stmt.y4)) return false;

    lattice::PolyVec zstar_modq = to_modq(p, ir.wit.zstar_centered);
    lattice::PolyVec y1 = lattice::mat_vec_mul(p, pk.A, zstar_modq);
    if (!polyvec_eq_modq(p, y1, ir.stmt.y1)) return false;

    const lattice::PolyMat& Acom = get_cached_commit_key(p, pk);
    lattice::PolyVec r_wit_modq = to_modq(p, ir.wit.r_wit_centered);
    lattice::PolyVec C_chk = Commit(p, Acom, r_wit_modq, zstar_modq);
    if (!polyvec_eq_modq(p, C_chk, ir.stmt.C)) return false;

    std::vector<uint8_t> packed_boundary_bits = pack_boundary_plane_bits(p, D, ir.wit.bit_wit, ir.stmt.L);
    *relation_digest = derive_relation_statement_digest(p, ir.stmt);
    *exact_digest = derive_relation_digest_v7(p, ir.relation_kind, *relation_digest, packed_boundary_bits);
    if (emit_proof_blob) {
        if (!proof_blob) return false;
        *proof_blob = build_relation_v7_minimal_blob(p, ir);
        if (proof_blob->empty()) return false;
    }
    return relation_digest->size() == 32 && exact_digest->size() == 32;
}

} 

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
    uint32_t interior_count) {
    RepairRelationIR ir;
    ir.relation_kind = PROOF_ENGINE_REPAIR_RELATION_FULL_V1;
    ir.stmt.cm = cm;
    ir.stmt.C = C;
    ir.stmt.r_small = r_small;
    ir.stmt.y1 = y1;
    ir.stmt.y3 = y3;
    ir.stmt.y4 = y4;
    ir.stmt.z_public_centered = z_public_centered;
    ir.stmt.delta_public_centered = delta_public_centered;
    ir.stmt.L = Lbits;
    ir.stmt.exact_u_max = exact_u_max;
    ir.stmt.boundary_count = boundary_count;
    ir.stmt.interior_count = interior_count;
    ir.wit.zstar_centered = zstar_centered;
    ir.wit.r_wit_centered = r_wit_centered;
    ir.wit.bit_wit = bit_wit;
    (void)p;
    return ir;
}

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
    uint32_t interior_count) {
    lattice::PolyVec delta_public_centered = derive_delta_centered_from_zstar(p, z_public_centered, zstar_centered);
    return BuildRepairRelationIR(p, cm, C, r_small, y1, y3, y4,
        z_public_centered, delta_public_centered, zstar_centered, r_wit_centered,
        bit_wit, Lbits, exact_u_max, boundary_count, interior_count);
}

bool BuildExactBackendArtifacts(const common::Params& p,
    const lattice::PublicKey& pk,
    const preauth::RepairTheta& th,
    const RepairRelationIR& ir,
    uint32_t backend_kind,
    std::vector<uint8_t>* relation_digest,
    std::vector<uint8_t>* exact_digest,
    std::vector<uint8_t>* proof_blob) {
    return compute_exact_backend_outputs(p, pk, th, ir, backend_kind,
        relation_digest, exact_digest, proof_blob, true);
}

static bool VerifyExactBackendArtifactsImpl(const common::Params& p,
    const lattice::PublicKey& pk,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& y_expected,
    const zk::RepairSigmaProof& pf,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& C,
    const lattice::PolyVec& r_small,
    const lattice::PolyVec& z_public_centered,
    const lattice::PolyVec* explicit_delta_public_centered) {
    if (pf.exact_digest.size() != 32) return false;

    RepairRelationIR ir;
    std::vector<uint8_t> stmt_digest_from_blob;
    std::vector<uint8_t> packed_boundary_bits_vx;
    std::vector<uint8_t> D_public_v7, M_public_v7;
    std::vector<int> off_public_v7;
    uint32_t max_u_v7 = 0;
    uint32_t backend_kind_from_blob = 0;
    bool decoded = false;
    uint32_t version = 0;
    if (!pf.exact_proof_blob.empty() && pf.exact_proof_blob.size() >= 4) {
        version = (uint32_t)pf.exact_proof_blob[0]
            | ((uint32_t)pf.exact_proof_blob[1] << 8)
            | ((uint32_t)pf.exact_proof_blob[2] << 16)
            | ((uint32_t)pf.exact_proof_blob[3] << 24);
        if (version == 3u) {
            decoded = decode_relation_v1_blob(p, pf.exact_proof_blob, ir, stmt_digest_from_blob, backend_kind_from_blob);
        } else if (version == 4u) {
            decoded = decode_relation_v2_compact_blob(p, th, z_public_centered, pf.exact_proof_blob, ir, stmt_digest_from_blob, backend_kind_from_blob);
        } else if (version == 5u) {
            decoded = decode_relation_v5_minimal_blob(p, th, cm, z_public_centered, pf.exact_proof_blob, ir, backend_kind_from_blob);
        } else if (version == 6u) {
            decoded = decode_relation_v6_minimal_blob(p, th, cm, z_public_centered, pf.exact_proof_blob, ir, backend_kind_from_blob, packed_boundary_bits_vx);
        } else if (version == 7u) {
            decoded = decode_relation_v7_minimal_blob(p, th, cm, z_public_centered, pf.exact_proof_blob, ir, backend_kind_from_blob, packed_boundary_bits_vx, D_public_v7, M_public_v7, off_public_v7, max_u_v7);
        }
    }
    if (!decoded) return false;
    if (backend_kind_from_blob != PROOF_ENGINE_ZK_BACKEND_RELATION_V1) return false;
    if (ir.relation_kind != PROOF_ENGINE_REPAIR_RELATION_FULL_V1) return false;

    ir.stmt.cm = cm;
    ir.stmt.C = C;
    ir.stmt.r_small = r_small;
    ir.stmt.y1 = y_expected;
    ir.stmt.z_public_centered = z_public_centered;
    if (explicit_delta_public_centered) {
        ir.stmt.delta_public_centered = *explicit_delta_public_centered;
    } else {
        ir.stmt.delta_public_centered = derive_delta_centered_from_zstar(p, z_public_centered, ir.wit.zstar_centered);
    }

    if (version == 7u) {
        fill_public_masked_views(p, D_public_v7, M_public_v7, off_public_v7, z_public_centered, ir.stmt.y3, ir.stmt.y4);
    } else {
        std::vector<uint8_t> D, M;
        std::vector<int> off_centered, sign;
        build_masks(p, th, z_public_centered, D, M, off_centered, sign);
        fill_public_masked_views(p, D, M, off_centered, z_public_centered, ir.stmt.y3, ir.stmt.y4);
    }

    if (ir.stmt.L != pf.L) return false;
    if (ir.stmt.exact_u_max != pf.exact_u_max) return false;
    if (!delta_matches_saturating_repair_rule(p, th, ir.stmt.z_public_centered,
            ir.stmt.delta_public_centered, ir.wit.zstar_centered)) return false;

    std::vector<uint8_t> relation_digest;
    std::vector<uint8_t> exact_digest;
    if (version == 6u || version == 7u) {
        relation_digest = derive_relation_statement_digest(p, ir.stmt);
        exact_digest = (version == 7u)
            ? derive_relation_digest_v7(p, ir.relation_kind, relation_digest, packed_boundary_bits_vx)
            : derive_relation_digest_v6(p, ir.relation_kind, relation_digest, packed_boundary_bits_vx);
        if (version == 7u) {
            if (max_u_v7 != ir.stmt.exact_u_max) return false;
        } else {
            if ((uint32_t)max_boundary_overflow(p, th, ir.stmt.z_public_centered, ir.wit.zstar_centered) != ir.stmt.exact_u_max) return false;
        }
        lattice::PolyVec zstar_modq = to_modq(p, ir.wit.zstar_centered);
        lattice::PolyVec y1_chk = lattice::mat_vec_mul(p, pk.A, zstar_modq);
        if (!polyvec_eq_modq(p, y1_chk, ir.stmt.y1)) return false;
        const lattice::PolyMat& Acom = get_cached_commit_key(p, pk);
        lattice::PolyVec r_wit_modq = to_modq(p, ir.wit.r_wit_centered);
        lattice::PolyVec C_chk = Commit(p, Acom, r_wit_modq, zstar_modq);
        if (!polyvec_eq_modq(p, C_chk, ir.stmt.C)) return false;
    } else {
        if (!compute_exact_backend_outputs(p, pk, th, ir, PROOF_ENGINE_ZK_BACKEND_RELATION_V1,
                &relation_digest, &exact_digest, nullptr, false)) return false;
    }
    if (exact_digest != pf.exact_digest) return false;
    if (!stmt_digest_from_blob.empty() && relation_digest != stmt_digest_from_blob) return false;
    return true;
}

bool VerifyExactBackendArtifacts(const common::Params& p,
    const lattice::PublicKey& pk,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& y_expected,
    const zk::RepairSigmaProof& pf,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& C,
    const lattice::PolyVec& r_small,
    const lattice::PolyVec& z_public_centered) {
    return VerifyExactBackendArtifactsImpl(p, pk, th, y_expected, pf, cm, C, r_small,
        z_public_centered, nullptr);
}

bool VerifyExactBackendArtifacts(const common::Params& p,
    const lattice::PublicKey& pk,
    const preauth::RepairTheta& th,
    const lattice::PolyVec& y_expected,
    const zk::RepairSigmaProof& pf,
    const preauth::CommitMsg& cm,
    const lattice::PolyVec& C,
    const lattice::PolyVec& r_small,
    const lattice::PolyVec& z_public_centered,
    const lattice::PolyVec& delta_public_centered) {
    return VerifyExactBackendArtifactsImpl(p, pk, th, y_expected, pf, cm, C, r_small,
        z_public_centered, &delta_public_centered);
}

} 
