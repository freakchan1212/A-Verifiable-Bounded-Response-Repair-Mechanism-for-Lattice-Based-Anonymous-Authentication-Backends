
#include "offload/coded_offload.h"

#include <vector>

#include "common/bytes.h"
#include "common/hash.h"

#include "lattice/mod_arith.h"
#include "lattice/poly.h"

#include "offload/freivalds.h"

namespace offload {

	static int modq_i(int x, int q) { return lattice::modq(x, q); }
	
	static lattice::Poly poly_scalar_mul(const common::Params& p, const lattice::Poly& a, int s) {
		lattice::Poly r(p.N);
		for (int i = 0; i < p.N; i++) {
			long long v = 1LL * modq_i(a.c[i], p.q) * modq_i(s, p.q);
			r.c[i] = modq_i((int)v, p.q);
		}
		return r;
	}
	
	static int pow_modq(int a, int e, int q) {
		long long r = 1;
		long long x = modq_i(a, q);
		while (e > 0) {
			if (e & 1) r = (r * x) % q;
			x = (x * x) % q;
			e >>= 1;
		}
		return (int)r;
	}
	
	static bool invert_matrix_modq(std::vector<std::vector<int>>& A, int q, std::vector<std::vector<int>>& inv) {
		const int n = (int)A.size();
		inv.assign(n, std::vector<int>(n, 0));
		for (int i = 0; i < n; i++) inv[i][i] = 1;
	
		for (int col = 0; col < n; col++) {
			int pivot = -1;
			for (int row = col; row < n; row++) {
				if (modq_i(A[row][col], q) != 0) { pivot = row; break; }
			}
			if (pivot == -1) return false;
	
			if (pivot != col) {
				std::swap(A[pivot], A[col]);
				std::swap(inv[pivot], inv[col]);
			}
	
			int a = modq_i(A[col][col], q);
	
			
			int t = 0, newt = 1;
			int r = q, newr = a;
			while (newr != 0) {
				int qq = r / newr;
				int tmp = t - qq * newt; t = newt; newt = tmp;
				tmp = r - qq * newr; r = newr; newr = tmp;
			}
			if (r > 1) return false;
			if (t < 0) t += q;
			int inva = t;
	
			for (int j = 0; j < n; j++) {
				A[col][j] = modq_i((int)(1LL * A[col][j] * inva), q);
				inv[col][j] = modq_i((int)(1LL * inv[col][j] * inva), q);
			}
	
			for (int row = 0; row < n; row++) {
				if (row == col) continue;
				int factor = modq_i(A[row][col], q);
				if (factor == 0) continue;
				for (int j = 0; j < n; j++) {
					A[row][j] = modq_i((int)(A[row][j] - 1LL * factor * A[col][j]), q);
					inv[row][j] = modq_i((int)(inv[row][j] - 1LL * factor * inv[col][j]), q);
				}
			}
		}
		return true;
	}
	
	bool CodedOffloadMatVec(const common::Params& p,
		const lattice::PolyMat& A,
		const lattice::PolyVec& x,
		lattice::PolyVec& y_out,
		OffloadStats& st,
		common::Rng& rng,
		const AdversaryConfig& adv) {
		
		int K = p.off_k;
		if (K < 1) K = 1;
		if (K > p.k) K = p.k;
	
		const int r = (p.off_r < 0 ? 0 : p.off_r);
		const int n = K + r;
	
		st = OffloadStats{};
		st.dim_k = p.k;
		st.k = K;
		st.r = r;
		st.n = n;
	
		if (p.k <= 0) return false;
		if (n < K) return false;
	
		
		AdversaryOutcome ao = Simulate(n, adv, rng);
		st.dropped = ao.dropped;
		st.corrupted = ao.corrupted_cnt;
	
		
		std::vector<int> evals(n);
		for (int m = 0; m < n; m++) evals[m] = m + 1;
	
		
		auto group_of = [&](int j)->int { return j % K; };
	
		
		std::vector<std::vector<int>> coeff(n, std::vector<int>(K, 0));
		for (int m = 0; m < n; m++) {
			for (int g = 0; g < K; g++) coeff[m][g] = pow_modq(evals[m], g, p.q);
		}
	
		
		std::vector<lattice::PolyVec> shard_out(n, lattice::vec_zero(p));
		for (int m = 0; m < n; m++) {
			lattice::PolyVec y_m = lattice::vec_zero(p);
	
			
			for (int i = 0; i < p.k; i++) {
				lattice::Poly acc = lattice::Poly::zero(p);
				for (int j = 0; j < p.k; j++) {
					int g = group_of(j);
					int s = coeff[m][g];
	
					lattice::Poly prod = lattice::mul_schoolbook(p, A.m[i][j], x.v[j]);
					prod = poly_scalar_mul(p, prod, s);
					acc = lattice::add(p, acc, prod);
				}
				y_m.v[i] = acc;
			}
	
			if (ao.corrupted[m]) {
				
				y_m.v[0].c[0] = modq_i(y_m.v[0].c[0] + 1, p.q);
			}
	
			shard_out[m] = y_m;
		}
	
		
		std::vector<int> used_ids;
		used_ids.reserve(K);
	
		st.waited = 0;
		st.tried = 0;
	
		for (int pos = 0; pos < n && (int)used_ids.size() < K; pos++) {
			int id = ao.arrival_order[pos];
			st.waited++;
			st.tried++;
			if (!ao.delivered[id]) continue;
			used_ids.push_back(id);
		}
	
		st.received = (int)used_ids.size();
		st.used = st.received;
	
		if ((int)used_ids.size() < K) {
			st.recovered = false;
			return false;
		}
	
		
		std::vector<std::vector<int>> C(K, std::vector<int>(K, 0));
		for (int row = 0; row < K; row++) {
			int eval = evals[used_ids[row]];
			for (int g = 0; g < K; g++) C[row][g] = pow_modq(eval, g, p.q);
		}
	
		
		std::vector<std::vector<int>> invC;
		std::vector<std::vector<int>> Ccopy = C;
		if (!invert_matrix_modq(Ccopy, p.q, invC)) {
			st.recovered = false;
			return false;
		}
	
		
		
		
		std::vector<int> w(K, 0);
		for (int col = 0; col < K; col++) {
			long long acc = 0;
			for (int g = 0; g < K; g++) acc += invC[g][col];
			w[col] = modq_i((int)acc, p.q);
		}
	
		
		y_out = lattice::vec_zero(p);
		for (int i = 0; i < p.k; i++) y_out.v[i] = lattice::Poly::zero(p);
	
		for (int i = 0; i < p.k; i++) {
			for (int c = 0; c < p.N; c++) {
				long long acc = 0;
				for (int row = 0; row < K; row++) {
					int id = used_ids[row];
					int ycod = modq_i(shard_out[id].v[i].c[c], p.q);
					acc += 1LL * w[row] * ycod;
				}
				y_out.v[i].c[c] = modq_i((int)acc, p.q);
			}
		}
	
		st.recovered = true;
	
		
		int rounds = p.off_freivalds;
		std::vector<uint8_t> fb;
		common::bytes::append_str(fb, "OffloadFreivalds-v2");
		common::bytes::append_u64(fb, p.seed);
		common::bytes::append_u32(fb, (uint32_t)K);
		common::bytes::append_u32(fb, (uint32_t)r);
		for (int id : used_ids) common::bytes::append_u32(fb, (uint32_t)id);
	
		uint64_t fr_seed = common::hash::hash64(fb);
		common::Rng fr_rng(fr_seed);
	
		bool ok = CheckMatVecFreivalds(p, A, x, y_out, rounds, fr_rng);
		st.freivalds_ok = ok;
		return ok;
	}

} 
