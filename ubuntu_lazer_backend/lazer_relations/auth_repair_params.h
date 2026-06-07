


#include "lazer.h"
static const limb_t _auth_repair_param_q_limbs[] = {144115188075856141UL};
static const int_t _auth_repair_param_q = {{(limb_t *)_auth_repair_param_q_limbs, 1, 0}};
static const limb_t _auth_repair_param_qminus1_limbs[] = {144115188075856140UL};
static const int_t _auth_repair_param_qminus1 = {{(limb_t *)_auth_repair_param_qminus1_limbs, 1, 0}};
static const limb_t _auth_repair_param_m_limbs[] = {78702097305UL};
static const int_t _auth_repair_param_m = {{(limb_t *)_auth_repair_param_m_limbs, 1, 0}};
static const limb_t _auth_repair_param_mby2_limbs[] = {39351048652UL};
static const int_t _auth_repair_param_mby2 = {{(limb_t *)_auth_repair_param_mby2_limbs, 1, 0}};
static const limb_t _auth_repair_param_gamma_limbs[] = {1831148UL};
static const int_t _auth_repair_param_gamma = {{(limb_t *)_auth_repair_param_gamma_limbs, 1, 0}};
static const limb_t _auth_repair_param_gammaby2_limbs[] = {915574UL};
static const int_t _auth_repair_param_gammaby2 = {{(limb_t *)_auth_repair_param_gammaby2_limbs, 1, 0}};
static const limb_t _auth_repair_param_pow2D_limbs[] = {4096UL};
static const int_t _auth_repair_param_pow2D = {{(limb_t *)_auth_repair_param_pow2D_limbs, 1, 0}};
static const limb_t _auth_repair_param_pow2Dby2_limbs[] = {2048UL};
static const int_t _auth_repair_param_pow2Dby2 = {{(limb_t *)_auth_repair_param_pow2Dby2_limbs, 1, 0}};
static const limb_t _auth_repair_param_Bsq_limbs[] = {1336206303880478UL, 0UL};
static const int_t _auth_repair_param_Bsq = {{(limb_t *)_auth_repair_param_Bsq_limbs, 2, 0}};
static const limb_t _auth_repair_param_scM1_limbs[] = {10959179632219693462UL, 5998924091375207691UL, 2UL};
static const int_t _auth_repair_param_scM1 = {{(limb_t *)_auth_repair_param_scM1_limbs, 3, 0}};
static const limb_t _auth_repair_param_scM2_limbs[] = {9365119720143779961UL, 11513158218042786489UL, 2UL};
static const int_t _auth_repair_param_scM2 = {{(limb_t *)_auth_repair_param_scM2_limbs, 3, 0}};
static const limb_t _auth_repair_param_scM3_limbs[] = {18034670371008010426UL, 647771802099886172UL, 1UL};
static const int_t _auth_repair_param_scM3 = {{(limb_t *)_auth_repair_param_scM3_limbs, 3, 0}};
static const limb_t _auth_repair_param_scM4_limbs[] = {9954670377577184598UL, 261316852771713275UL, 1UL};
static const int_t _auth_repair_param_scM4 = {{(limb_t *)_auth_repair_param_scM4_limbs, 3, 0}};
static const limb_t _auth_repair_param_stdev1sq_limbs[] = {41274635715UL, 0UL};
static const int_t _auth_repair_param_stdev1sq = {{(limb_t *)_auth_repair_param_stdev1sq_limbs, 2, 0}};
static const limb_t _auth_repair_param_stdev2sq_limbs[] = {40307261UL, 0UL};
static const int_t _auth_repair_param_stdev2sq = {{(limb_t *)_auth_repair_param_stdev2sq_limbs, 2, 0}};
static const limb_t _auth_repair_param_stdev3sq_limbs[] = {40307261UL, 0UL};
static const int_t _auth_repair_param_stdev3sq = {{(limb_t *)_auth_repair_param_stdev3sq_limbs, 2, 0}};
static const limb_t _auth_repair_param_stdev4sq_limbs[] = {165098542858UL, 0UL};
static const int_t _auth_repair_param_stdev4sq = {{(limb_t *)_auth_repair_param_stdev4sq_limbs, 2, 0}};
static const limb_t _auth_repair_param_inv2_limbs[] = {72057594037928070UL};
static const int_t _auth_repair_param_inv2 = {{(limb_t *)_auth_repair_param_inv2_limbs, 1, 1}};
static const limb_t _auth_repair_param_inv4_limbs[] = {36028797018964035UL};
static const int_t _auth_repair_param_inv4 = {{(limb_t *)_auth_repair_param_inv4_limbs, 1, 1}};
static const unsigned int _auth_repair_param_n[1] = {116};
static const limb_t _auth_repair_param_Bz3sqr_limbs[] = {27753065054UL, 0UL};
static const int_t _auth_repair_param_Bz3sqr = {{(limb_t *)_auth_repair_param_Bz3sqr_limbs, 2, 0}};
static const limb_t _auth_repair_param_Bz4_limbs[] = {6501171UL};
static const int_t _auth_repair_param_Bz4 = {{(limb_t *)_auth_repair_param_Bz4_limbs, 1, 0}};
static const limb_t _auth_repair_param_Pmodq_limbs[] = {68553955592441063UL};
static const int_t _auth_repair_param_Pmodq = {{(limb_t *)_auth_repair_param_Pmodq_limbs, 1, 1}};
static const limb_t _auth_repair_param_l2Bsq0_limbs[] = {8192UL};
static const int_t _auth_repair_param_l2Bsq0 = {{(limb_t *)_auth_repair_param_l2Bsq0_limbs, 1, 0}};
static const limb_t _auth_repair_param_Ppmodq_0_limbs[] = {114349195861747UL};
static const int_t _auth_repair_param_Ppmodq_0 = {{(limb_t *)_auth_repair_param_Ppmodq_0_limbs, 1, 1}};
static const limb_t _auth_repair_param_Ppmodq_1_limbs[] = {114349200335566UL};
static const int_t _auth_repair_param_Ppmodq_1 = {{(limb_t *)_auth_repair_param_Ppmodq_1_limbs, 1, 1}};
static const limb_t _auth_repair_param_Ppmodq_2_limbs[] = {114349204466872UL};
static const int_t _auth_repair_param_Ppmodq_2 = {{(limb_t *)_auth_repair_param_Ppmodq_2_limbs, 1, 1}};
static const int_srcptr _auth_repair_param_l2Bsq[] = {_auth_repair_param_l2Bsq0};
static const int_srcptr _auth_repair_param_Ppmodq[] = {_auth_repair_param_Ppmodq_0, _auth_repair_param_Ppmodq_1, _auth_repair_param_Ppmodq_2};
static const polyring_t _auth_repair_param_ring = {{_auth_repair_param_q, 64, 58, 6, moduli_d64, 3, _auth_repair_param_Pmodq, _auth_repair_param_Ppmodq, _auth_repair_param_inv2}};
static const dcompress_params_t _auth_repair_param_dcomp = {{ _auth_repair_param_q, _auth_repair_param_qminus1, _auth_repair_param_m, _auth_repair_param_mby2, _auth_repair_param_gamma, _auth_repair_param_gammaby2, _auth_repair_param_pow2D, _auth_repair_param_pow2Dby2, 12, 1, 37 }};
static const abdlop_params_t _auth_repair_param_tbox = {{ _auth_repair_param_ring, _auth_repair_param_dcomp, 117, 62, 0, 12, 14, _auth_repair_param_Bsq, 1, 8, 5, 140, 1, 17, _auth_repair_param_scM1, _auth_repair_param_stdev1sq, 2, 12, _auth_repair_param_scM2, _auth_repair_param_stdev2sq}};
static const abdlop_params_t _auth_repair_param_quad_eval_ = {{ _auth_repair_param_ring, _auth_repair_param_dcomp, 117, 62, 9, 3, 14, _auth_repair_param_Bsq, 1, 8, 5, 140, 1, 17, _auth_repair_param_scM1, _auth_repair_param_stdev1sq, 2, 12, _auth_repair_param_scM2, _auth_repair_param_stdev2sq}};
static const abdlop_params_t _auth_repair_param_quad_many_ = {{ _auth_repair_param_ring, _auth_repair_param_dcomp, 117, 62, 11, 1, 14, _auth_repair_param_Bsq, 1, 8, 5, 140, 1, 17, _auth_repair_param_scM1, _auth_repair_param_stdev1sq, 2, 12, _auth_repair_param_scM2, _auth_repair_param_stdev2sq}};
static const lnp_quad_eval_params_t _auth_repair_param_quad_eval = {{ _auth_repair_param_quad_eval_, _auth_repair_param_quad_many_, 4}};
static const lnp_tbox_params_t _auth_repair_param = {{ _auth_repair_param_tbox, _auth_repair_param_quad_eval, 0, _auth_repair_param_n, 88, 1, 117, 2, 12, _auth_repair_param_scM3, _auth_repair_param_stdev3sq, 2, 18, _auth_repair_param_scM4, _auth_repair_param_stdev4sq, _auth_repair_param_Bz3sqr, _auth_repair_param_Bz4, &_auth_repair_param_l2Bsq[0], _auth_repair_param_inv4, 38956UL }};

static const unsigned int auth_repair_param_Es0[116] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115};
static const unsigned int *auth_repair_param_Es[1] = { auth_repair_param_Es0, };
static const unsigned int auth_repair_param_Es_nrows[1] = {116};

static const limb_t auth_repair_param_p_limbs[] = {4294962689UL};
static const int_t auth_repair_param_p = {{(limb_t *)auth_repair_param_p_limbs, 1, 0}};
static const limb_t auth_repair_param_pinv_limbs[] = {20339743370493182UL};
static const int_t auth_repair_param_pinv = {{(limb_t *)auth_repair_param_pinv_limbs, 1, 1}};
static const unsigned int auth_repair_param_s1_indices[29] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28};
static const lin_params_t auth_repair_param = {{ _auth_repair_param, 256, auth_repair_param_p, auth_repair_param_pinv, 4, auth_repair_param_s1_indices, 29, NULL, 0,  NULL, 0, auth_repair_param_Es, auth_repair_param_Es_nrows, NULL, NULL }};

