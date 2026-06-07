
#include "repair.h"
#include <cstdlib> 


std::vector<int> deterministic_repair(std::vector<int> vec, int B, int q) {
	int half_q = q / 2;
	for (int& x : vec) {
		x %= q;
		if (x > half_q) x -= q;
		if (x < -half_q) x += q;
		
		if (x > B) x = B;
		if (x < -B) x = -B;
		
		
	}
	return vec;
}


bool check_norm(const std::vector<int>& vec, int B) {
	for (int x : vec) {
		if (x > B || x < -B) return false;
	}
	return true;
}
