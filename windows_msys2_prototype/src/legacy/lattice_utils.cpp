
#include "lattice_utils.h"
#include <vector>
#include <algorithm>


int modq(int x) {
	int m = x % Q;
	if (m < 0) m += Q;
	return m;
}


int modInv(int a, int q) {
	a %= q; if (a < 0) a += q;
	int b = q, x0 = 1, x1 = 0;
	while (b != 0) {
		int t = a / b;
		a -= t * b; std::swap(a, b);
		x0 -= t * x1; std::swap(x0, x1);
	}
	if (a != 1) return 0; 
	if (x0 < 0) x0 += q;
	return x0;
}


std::vector<int> mat_vec_mul(const std::vector<std::vector<int>>& A,
	const std::vector<int>& x, int q) {
	int n = A.size();
	int m = x.size();
	std::vector<int> res(n, 0);
	for (int i = 0; i < n; ++i) {
		long long sum = 0;
		for (int j = 0; j < m; ++j) {
			sum += (long long)A[i][j] * x[j];
		}
		int t = sum % q;
		if (t < 0) t += q;
		res[i] = t;
	}
	return res;
}
