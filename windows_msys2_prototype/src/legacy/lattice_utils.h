
#ifndef LATTICE_UTILS_H
#define LATTICE_UTILS_H

#include <vector>


static const int Q = 97;


int modq(int x);

int modInv(int a, int q);

std::vector<int> mat_vec_mul(const std::vector<std::vector<int>>& A,
	const std::vector<int>& x, int q);

#endif 
