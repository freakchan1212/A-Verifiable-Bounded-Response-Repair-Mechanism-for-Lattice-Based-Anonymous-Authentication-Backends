
#include "mod_arith.h"

namespace lattice {

	
	
	
	int modq(int x, int q) {
		int r = x % q;      
		if (r < 0) r += q;  
		return r;
	}
	
	
	
	
	
	
	
	
	int center_q(int x, int q) {
		int r = modq(x, q);  
		int half = q / 2;    
		if (r > half) r -= q; 
		return r;
	}
	
	
	int addq(int a, int b, int q) {
		return modq(a + b, q);
	}
	
	
	int subq(int a, int b, int q) {
		return modq(a - b, q);
	}

} 
