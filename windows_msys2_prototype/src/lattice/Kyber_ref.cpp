
#include "kyber_ref.h"
#include "mod_arith.h"

#include <cstdint>

namespace lattice::kyber_ref {

	static constexpr int KYBER_N = 256;
	static constexpr int KYBER_Q = 3329;
	static constexpr int KYBER_QINV = 62209; 
	static constexpr int KYBER_F = 1441;     
	
	static const int16_t KYBER_ZETAS[128] = {
		-1044, -758,  -359, -1517, 1493,  1422,  287,   202,
		-171,   622,  1577,  182,   962, -1202, -1474, 1468,
		 573, -1325,   264,   383,  -829, 1458, -1602, -130,
		-681,  1017,   732,   608, -1542,  411,  -205, -1571,
		1223,   652,  -552,  1015, -1293, 1491,  -282, -1544,
		 516,    -8,  -320,  -666, -1618, -1162,  126,  1469,
		-853,   -90,  -271,   830,   107, -1421, -247,  -951,
		-398,   961, -1508,  -725,   448, -1065,  677, -1275,
		-1103,  430,   555,   843, -1251,  871,  1550,   105,
		 422,   587,   177,  -235,  -291, -460, 1574,  1653,
		-246,   778,  1159,  -147,  -777, 1483, -602,  1119,
		-1590,  644,  -872,   349,   418,  329,  -156,   -75,
		 817,  1097,   603,   610,  1322, -1285, -1465,  384,
		-1215, -136,  1218, -1335,  -874,  220, -1187, -1659,
		-1185, -1530, -1278,   794, -1510, -854,  -870,   478,
		-108,  -308,   996,   991,   958, -1460, 1522,  1628
	};
	
	bool supported(const common::Params& p) {
		return (p.N == KYBER_N && p.q == KYBER_Q);
	}
	
	static inline int16_t montgomery_reduce(int32_t a) {
		int16_t t;
		t = (int16_t)a * (int16_t)KYBER_QINV;
		t = (int16_t)((a - (int32_t)t * KYBER_Q) >> 16);
		return t;
	}
	
	static inline int16_t barrett_reduce(int16_t a) {
		const int16_t v = (int16_t)(((1U << 26) + KYBER_Q / 2) / KYBER_Q);
		int16_t t = (int16_t)(((int32_t)v * a + (1 << 25)) >> 26);
		t = (int16_t)(t * KYBER_Q);
		return (int16_t)(a - t);
	}
	
	static inline int16_t fqmul(int16_t a, int16_t b) {
		return montgomery_reduce((int32_t)a * b);
	}
	
	static void ntt_ref(int16_t r[KYBER_N]) {
		unsigned int len, start, j, k;
		int16_t t, zeta;
	
		k = 1;
		for (len = 128; len >= 2; len >>= 1) {
			for (start = 0; start < KYBER_N; start = j + len) {
				zeta = KYBER_ZETAS[k++];
				for (j = start; j < start + len; j++) {
					t = fqmul(zeta, r[j + len]);
					r[j + len] = (int16_t)(r[j] - t);
					r[j] = (int16_t)(r[j] + t);
				}
			}
		}
	}
	
	static void invntt_ref(int16_t r[KYBER_N]) {
		unsigned int start, len, j, k;
		int16_t t, zeta;
		const int16_t f = KYBER_F;
	
		k = 127;
		for (len = 2; len <= 128; len <<= 1) {
			for (start = 0; start < KYBER_N; start = j + len) {
				zeta = KYBER_ZETAS[k--];
				for (j = start; j < start + len; j++) {
					t = r[j];
					r[j] = barrett_reduce((int16_t)(t + r[j + len]));
					r[j + len] = (int16_t)(r[j + len] - t);
					r[j + len] = fqmul(zeta, r[j + len]);
				}
			}
		}
	
		for (j = 0; j < KYBER_N; j++) {
			r[j] = fqmul(r[j], f);
		}
	}
	
	static void basemul_ref(int16_t r[2], const int16_t a[2], const int16_t b[2], int16_t zeta) {
		r[0] = fqmul(a[1], b[1]);
		r[0] = fqmul(r[0], zeta);
		r[0] = (int16_t)(r[0] + fqmul(a[0], b[0]));
		r[1] = fqmul(a[0], b[1]);
		r[1] = (int16_t)(r[1] + fqmul(a[1], b[0]));
	}
	
	static void poly_reduce_ref(Poly& a) {
		for (int i = 0; i < KYBER_N; i++) {
			a.c[(size_t)i] = (Poly::coeff_t)barrett_reduce((int16_t)a.c[(size_t)i]);
		}
	}
	
	void poly_ntt(const common::Params& p, Poly& a) {
		(void)p;
		int16_t r[KYBER_N];
		for (int i = 0; i < KYBER_N; i++) {
			
			r[i] = (int16_t)modq((int)a.c[(size_t)i], KYBER_Q);
		}
		ntt_ref(r);
		for (int i = 0; i < KYBER_N; i++) {
			a.c[(size_t)i] = (Poly::coeff_t)r[i];
		}
		poly_reduce_ref(a);
	}
	
	void poly_invntt_tomont(const common::Params& p, Poly& a) {
		(void)p;
		int16_t r[KYBER_N];
		for (int i = 0; i < KYBER_N; i++) {
			r[i] = (int16_t)a.c[(size_t)i];
		}
		invntt_ref(r);
		for (int i = 0; i < KYBER_N; i++) {
			a.c[(size_t)i] = (Poly::coeff_t)modq((int)r[i], KYBER_Q);
		}
	}
	
	void poly_basemul_montgomery(const common::Params& p, Poly& r, const Poly& a, const Poly& b) {
		(void)p;
		r = Poly(KYBER_N);
	
		for (int i = 0; i < KYBER_N / 4; i++) {
			int16_t aa0[2] = {
				(int16_t)a.c[(size_t)(4 * i + 0)],
				(int16_t)a.c[(size_t)(4 * i + 1)]
			};
			int16_t bb0[2] = {
				(int16_t)b.c[(size_t)(4 * i + 0)],
				(int16_t)b.c[(size_t)(4 * i + 1)]
			};
			int16_t rr0[2];
			basemul_ref(rr0, aa0, bb0, KYBER_ZETAS[64 + i]);
			r.c[(size_t)(4 * i + 0)] = (Poly::coeff_t)rr0[0];
			r.c[(size_t)(4 * i + 1)] = (Poly::coeff_t)rr0[1];
	
			int16_t aa1[2] = {
				(int16_t)a.c[(size_t)(4 * i + 2)],
				(int16_t)a.c[(size_t)(4 * i + 3)]
			};
			int16_t bb1[2] = {
				(int16_t)b.c[(size_t)(4 * i + 2)],
				(int16_t)b.c[(size_t)(4 * i + 3)]
			};
			int16_t rr1[2];
			basemul_ref(rr1, aa1, bb1, (int16_t)(-KYBER_ZETAS[64 + i]));
			r.c[(size_t)(4 * i + 2)] = (Poly::coeff_t)rr1[0];
			r.c[(size_t)(4 * i + 3)] = (Poly::coeff_t)rr1[1];
		}
	}
	
	Poly mul_poly(const common::Params& p, const Poly& a, const Poly& b) {
		Poly A = a;
		Poly B = b;
		Poly C(KYBER_N);
	
		poly_ntt(p, A);
		poly_ntt(p, B);
		poly_basemul_montgomery(p, C, A, B);
		poly_invntt_tomont(p, C);
	
		
		for (int i = 0; i < KYBER_N; i++) {
			C.c[(size_t)i] = (Poly::coeff_t)modq((int)C.c[(size_t)i], KYBER_Q);
		}
		return C;
	}

} 
