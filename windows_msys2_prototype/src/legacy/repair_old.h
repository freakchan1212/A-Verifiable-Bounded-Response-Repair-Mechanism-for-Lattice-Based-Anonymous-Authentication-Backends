
#ifndef REPAIR_H
#define REPAIR_H

#include <vector>


std::vector<int> deterministic_repair(std::vector<int> vec, int B, int q);

bool check_norm(const std::vector<int>& vec, int B);

#endif 
