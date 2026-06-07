
#include "ntt.h"
#include "kyber_ref.h"
#include "mod_arith.h"

#include <vector>
#include <cstdint>
#include <algorithm>

#include "common/bytes.h"
#include "common/hash.h"
#include "common/rng.h"
#include "offload/adversary.h"
#include "offload/ntt_offload.h"

namespace lattice::ntt {

	static bool is_pow2(int n) { return n > 0 && (n & (n - 1)) == 0; }
	
	static int modpow(int a, int e, int q) {
		long long r = 1;
		long long x = modq(a, q);
		while (e > 0) {
			if (e & 1) r = (r * x) % q;
			x = (x * x) % q;
			e >>= 1;
		}
		return (int)r;
	}
	
	static std::vector<int> factorize(int x) {
		std::vector<int> f;
		for (int p = 2; p * p <= x; p++) {
			if (x % p == 0) {
				f.push_back(p);
				while (x % p == 0) x /= p;
			}
		}
		if (x > 1) f.push_back(x);
		return f;
	}
	
	static int find_primitive_root(int q) {
		int phi = q - 1;
		std::vector<int> factors = factorize(phi);
		for (int g = 2; g < q; g++) {
			bool ok = true;
			for (int p : factors) {
				if (modpow(g, phi / p, q) == 1) { ok = false; break; }
			}
			if (ok) return g;
		}
		return 0;
	}
	
	static bool supports_generic_negacyclic(const common::Params& p) {
		if (!is_pow2(p.N)) return false;
		return ((p.q - 1) % (2 * p.N) == 0);
	}
	
	bool supported(const common::Params& p) {
		if (!is_pow2(p.N)) return false;
		if (lattice::kyber_ref::supported(p)) return true;
		return supports_generic_negacyclic(p);
	}
	
	
	
	
	
	struct GenericNttCtx {
		int N = 0;
		int q = 0;
		int omega = 0;
		int omega_inv = 0;
		int psi = 0;
		int psi_inv = 0;
		int n_inv = 0;
		std::vector<int> bitrev;
		std::vector<int> psi_pows;
		std::vector<int> psi_inv_pows;
	};
	
	static bool egcd_inv(int a, int mod, int& out) {
		int t = 0, newt = 1;
		int r = mod, newr = modq(a, mod);
		while (newr != 0) {
			int q = r / newr;
			int tmp = t - q * newt; t = newt; newt = tmp;
			tmp = r - q * newr; r = newr; newr = tmp;
		}
		if (r > 1) return false;
		if (t < 0) t += mod;
		out = t;
		return true;
	}
	
	static bool build_generic_ctx(const common::Params& p, GenericNttCtx& ctx) {
		ctx = GenericNttCtx{};
		ctx.N = p.N;
		ctx.q = p.q;
	
		if (!supports_generic_negacyclic(p)) return false;
	
		int g = find_primitive_root(p.q);
		if (g == 0) return false;
	
		ctx.psi = modpow(g, (p.q - 1) / (2 * p.N), p.q);
		ctx.omega = modq((int)(1LL * ctx.psi * ctx.psi), p.q);
	
		if (!egcd_inv(ctx.psi, p.q, ctx.psi_inv)) return false;
		if (!egcd_inv(ctx.omega, p.q, ctx.omega_inv)) return false;
		if (!egcd_inv(p.N, p.q, ctx.n_inv)) return false;
	
		int logN = 0;
		while ((1 << logN) < p.N) logN++;
	
		ctx.bitrev.resize(p.N, 0);
		for (int i = 0; i < p.N; i++) {
			int x = i, r = 0;
			for (int b = 0; b < logN; b++) {
				r = (r << 1) | (x & 1);
				x >>= 1;
			}
			ctx.bitrev[i] = r;
		}
	
		ctx.psi_pows.resize(p.N, 1);
		ctx.psi_inv_pows.resize(p.N, 1);
		for (int i = 1; i < p.N; i++) {
			ctx.psi_pows[i] = modq((int)(1LL * ctx.psi_pows[i - 1] * ctx.psi), p.q);
			ctx.psi_inv_pows[i] = modq((int)(1LL * ctx.psi_inv_pows[i - 1] * ctx.psi_inv), p.q);
		}
	
		return true;
	}
	
	static GenericNttCtx& get_generic_ctx(const common::Params& p) {
		static GenericNttCtx ctx;
		static bool inited = false;
		if (!inited || ctx.N != p.N || ctx.q != p.q) {
			build_generic_ctx(p, ctx);
			inited = true;
		}
		return ctx;
	}
	
	static void bitrev_perm(const GenericNttCtx& ctx, std::vector<int>& a) {
		for (int i = 0; i < ctx.N; i++) {
			int j = ctx.bitrev[i];
			if (j > i) std::swap(a[i], a[j]);
		}
	}
	
	static void ntt_cyclic_generic(const GenericNttCtx& ctx, std::vector<int>& a, bool inverse_flag) {
		const int N = ctx.N;
		const int q = ctx.q;
		bitrev_perm(ctx, a);
	
		for (int len = 1; len < N; len <<= 1) {
			int step = N / (2 * len);
			int wlen = inverse_flag ? modpow(ctx.omega_inv, step, q) : modpow(ctx.omega, step, q);
	
			for (int i = 0; i < N; i += 2 * len) {
				int w = 1;
				for (int j = 0; j < len; j++) {
					int u = a[i + j];
					int v = modq((int)(1LL * a[i + j + len] * w), q);
					a[i + j] = modq(u + v, q);
					a[i + j + len] = modq(u - v, q);
					w = modq((int)(1LL * w * wlen), q);
				}
			}
		}
	
		if (inverse_flag) {
			for (int i = 0; i < N; i++) a[i] = modq((int)(1LL * a[i] * ctx.n_inv), q);
		}
	}
	
	static void generic_forward(const common::Params& p, Poly& a) {
		GenericNttCtx& ctx = get_generic_ctx(p);
	
		std::vector<int> v(p.N);
		for (int i = 0; i < p.N; i++) {
			int x = modq((int)a.c[(size_t)i], p.q);
			v[(size_t)i] = modq((int)(1LL * x * ctx.psi_pows[(size_t)i]), p.q);
		}
	
		ntt_cyclic_generic(ctx, v, false);
	
		for (int i = 0; i < p.N; i++) a.c[(size_t)i] = (Poly::coeff_t)modq(v[(size_t)i], p.q);
	}
	
	static void generic_inverse(const common::Params& p, Poly& a) {
		GenericNttCtx& ctx = get_generic_ctx(p);
	
		std::vector<int> v(p.N);
		for (int i = 0; i < p.N; i++) v[(size_t)i] = modq((int)a.c[(size_t)i], p.q);
	
		ntt_cyclic_generic(ctx, v, true);
	
		for (int i = 0; i < p.N; i++) {
			v[(size_t)i] = modq((int)(1LL * v[(size_t)i] * ctx.psi_inv_pows[(size_t)i]), p.q);
			a.c[(size_t)i] = (Poly::coeff_t)modq(v[(size_t)i], p.q);
		}
	}
	
	static void generic_mul_freq(const common::Params& p, const Poly& A, const Poly& B, Poly& C) {
		C = Poly(p.N);
		for (int i = 0; i < p.N; i++) {
			long long prod = 1LL * modq((int)A.c[(size_t)i], p.q) * modq((int)B.c[(size_t)i], p.q);
			C.c[(size_t)i] = (Poly::coeff_t)modq((int)prod, p.q);
		}
	}
	
	static Poly mul_ntt_local_no_offload(const common::Params& p, const Poly& a, const Poly& b) {
		if (!p.use_ntt || !supported(p)) {
			return mul_schoolbook(p, a, b);
		}
	
		if (lattice::kyber_ref::supported(p)) {
			return lattice::kyber_ref::mul_poly(p, a, b);
		}
	
		Poly A = a;
		Poly B = b;
		generic_forward(p, A);
		generic_forward(p, B);
	
		Poly C(p.N);
		generic_mul_freq(p, A, B, C);
	
		generic_inverse(p, C);
		return C;
	}
	
	void forward(const common::Params& p, Poly& a) {
		if (!p.use_ntt || !supported(p)) return;
	
		if (lattice::kyber_ref::supported(p)) {
			lattice::kyber_ref::poly_ntt(p, a);
			return;
		}
	
		generic_forward(p, a);
	}
	
	void inverse(const common::Params& p, Poly& a) {
		if (!p.use_ntt || !supported(p)) return;
	
		if (lattice::kyber_ref::supported(p)) {
			lattice::kyber_ref::poly_invntt_tomont(p, a);
			return;
		}
	
		generic_inverse(p, a);
	}
	
	void mul_freq(const common::Params& p, const Poly& A, const Poly& B, Poly& C) {
		if (lattice::kyber_ref::supported(p)) {
			lattice::kyber_ref::poly_basemul_montgomery(p, C, A, B);
			return;
		}
		generic_mul_freq(p, A, B, C);
	}
	
	Poly mul_ntt(const common::Params& p, const Poly& a, const Poly& b) {
		if (!p.use_ntt || !supported(p)) {
			return mul_schoolbook(p, a, b);
		}
	
		if (p.offload_proto == 2) {
			::offload::AdversaryConfig adv;
			adv.straggler_pct = p.off_straggler_pct;
			adv.malicious_pct = p.off_malicious_pct;
	
			std::vector<uint8_t> sb;
			::common::bytes::append_str(sb, "PolyMulOffloadSeed-v4");
			::common::bytes::append_u64(sb, p.seed);
			for (int i = 0; i < p.N; i++) ::common::bytes::append_i16(sb, (int16_t)center_q((int)a.c[(size_t)i], p.q));
			for (int i = 0; i < p.N; i++) ::common::bytes::append_i16(sb, (int16_t)center_q((int)b.c[(size_t)i], p.q));
	
			uint64_t seed = ::common::hash::hash64(sb);
			::common::Rng rng(seed);
	
			Poly c(p.N);
			::offload::NttOffloadStats st;
			(void)::offload::PolyMulOffload(p, a, b, c, rng, adv, &st);
			return c;
		}
	
		return mul_ntt_local_no_offload(p, a, b);
	}

} 
