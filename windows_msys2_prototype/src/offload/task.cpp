
#include "offload/task.h"
#include "lattice/mod_arith.h"
#include "lattice/poly.h"

namespace offload {

	static lattice::Poly poly_scalar_mul(const common::Params& p, const lattice::Poly& a, int s) {
		lattice::Poly r(p.N);
		for (int i = 0; i < p.N; i++) {
			long long v = 1LL * lattice::modq(a.c[i], p.q) * lattice::modq(s, p.q);
			r.c[i] = lattice::modq((int)v, p.q);
		}
		return r;
	}
	
	static int pow_modq(int a, int e, int q) {
		long long r = 1;
		long long x = lattice::modq(a, q);
		while (e > 0) {
			if (e & 1) r = (r * x) % q;
			x = (x * x) % q;
			e >>= 1;
		}
		return (int)r;
	}
	
	EncodedRowTask MakeEncodedRowTask(const common::Params& p,
		const lattice::PolyMat& A,
		int shard_id,
		int eval) {
		EncodedRowTask task;
		task.shard_id = shard_id;
		task.eval = eval;
		task.coeff_row.resize(p.k);
	
		
		for (int i = 0; i < p.k; i++) task.coeff_row[i] = pow_modq(eval, i, p.q);
	
		task.rowcomb = lattice::vec_zero(p);
		for (int j = 0; j < p.k; j++) {
			lattice::Poly acc = lattice::Poly::zero(p);
			for (int i = 0; i < p.k; i++) {
				acc = lattice::add(p, acc, poly_scalar_mul(p, A.m[i][j], task.coeff_row[i]));
			}
			task.rowcomb.v[j] = acc;
		}
	
		return task;
	}
	
	lattice::Poly RunEncodedRowTask(const common::Params& p,
		const EncodedRowTask& task,
		const lattice::PolyVec& x) {
		lattice::Poly out = lattice::Poly::zero(p);
		for (int j = 0; j < p.k; j++) {
			lattice::Poly prod = lattice::mul_schoolbook(p, task.rowcomb.v[j], x.v[j]);
			out = lattice::add(p, out, prod);
		}
		return out;
	}

} 
