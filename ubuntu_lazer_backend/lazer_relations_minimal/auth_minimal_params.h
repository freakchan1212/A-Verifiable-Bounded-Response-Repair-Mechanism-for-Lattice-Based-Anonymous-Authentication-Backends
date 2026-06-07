


#include "lazer.h"
static const limb_t _auth_minimal_param_q_limbs[] = {18014398509482461UL};
static const int_t _auth_minimal_param_q = {{(limb_t *)_auth_minimal_param_q_limbs, 1, 0}};
static const limb_t _auth_minimal_param_qminus1_limbs[] = {18014398509482460UL};
static const int_t _auth_minimal_param_qminus1 = {{(limb_t *)_auth_minimal_param_qminus1_limbs, 1, 0}};
static const limb_t _auth_minimal_param_m_limbs[] = {42468759747UL};
static const int_t _auth_minimal_param_m = {{(limb_t *)_auth_minimal_param_m_limbs, 1, 0}};
static const limb_t _auth_minimal_param_mby2_limbs[] = {21234379873UL};
static const int_t _auth_minimal_param_mby2 = {{(limb_t *)_auth_minimal_param_mby2_limbs, 1, 0}};
static const limb_t _auth_minimal_param_gamma_limbs[] = {424180UL};
static const int_t _auth_minimal_param_gamma = {{(limb_t *)_auth_minimal_param_gamma_limbs, 1, 0}};
static const limb_t _auth_minimal_param_gammaby2_limbs[] = {212090UL};
static const int_t _auth_minimal_param_gammaby2 = {{(limb_t *)_auth_minimal_param_gammaby2_limbs, 1, 0}};
static const limb_t _auth_minimal_param_pow2D_limbs[] = {512UL};
static const int_t _auth_minimal_param_pow2D = {{(limb_t *)_auth_minimal_param_pow2D_limbs, 1, 0}};
static const limb_t _auth_minimal_param_pow2Dby2_limbs[] = {256UL};
static const int_t _auth_minimal_param_pow2Dby2 = {{(limb_t *)_auth_minimal_param_pow2Dby2_limbs, 1, 0}};
static const limb_t _auth_minimal_param_Bsq_limbs[] = {59338051388054UL, 0UL};
static const int_t _auth_minimal_param_Bsq = {{(limb_t *)_auth_minimal_param_Bsq_limbs, 2, 0}};
static const limb_t _auth_minimal_param_scM1_limbs[] = {10959179632219693462UL, 5998924091375207691UL, 2UL};
static const int_t _auth_minimal_param_scM1 = {{(limb_t *)_auth_minimal_param_scM1_limbs, 3, 0}};
static const limb_t _auth_minimal_param_scM2_limbs[] = {8624154020888717866UL, 9305399036312114916UL, 2UL};
static const int_t _auth_minimal_param_scM2 = {{(limb_t *)_auth_minimal_param_scM2_limbs, 3, 0}};
static const limb_t _auth_minimal_param_scM3_limbs[] = {18034670371008010426UL, 647771802099886172UL, 1UL};
static const int_t _auth_minimal_param_scM3 = {{(limb_t *)_auth_minimal_param_scM3_limbs, 3, 0}};
static const limb_t _auth_minimal_param_scM4_limbs[] = {16450470016763750670UL, 319208815437881850UL, 1UL};
static const int_t _auth_minimal_param_scM4 = {{(limb_t *)_auth_minimal_param_scM4_limbs, 3, 0}};
static const limb_t _auth_minimal_param_stdev1sq_limbs[] = {41274635715UL, 0UL};
static const int_t _auth_minimal_param_stdev1sq = {{(limb_t *)_auth_minimal_param_stdev1sq_limbs, 2, 0}};
static const limb_t _auth_minimal_param_stdev2sq_limbs[] = {40307261UL, 0UL};
static const int_t _auth_minimal_param_stdev2sq = {{(limb_t *)_auth_minimal_param_stdev2sq_limbs, 2, 0}};
static const limb_t _auth_minimal_param_stdev3sq_limbs[] = {40307261UL, 0UL};
static const int_t _auth_minimal_param_stdev3sq = {{(limb_t *)_auth_minimal_param_stdev3sq_limbs, 2, 0}};
static const limb_t _auth_minimal_param_stdev4sq_limbs[] = {2579664732UL, 0UL};
static const int_t _auth_minimal_param_stdev4sq = {{(limb_t *)_auth_minimal_param_stdev4sq_limbs, 2, 0}};
static const limb_t _auth_minimal_param_inv2_limbs[] = {9007199254741230UL};
static const int_t _auth_minimal_param_inv2 = {{(limb_t *)_auth_minimal_param_inv2_limbs, 1, 1}};
static const limb_t _auth_minimal_param_inv4_limbs[] = {4503599627370615UL};
static const int_t _auth_minimal_param_inv4 = {{(limb_t *)_auth_minimal_param_inv4_limbs, 1, 1}};
static const unsigned int _auth_minimal_param_n[1] = {16};
static const limb_t _auth_minimal_param_Bz3sqr_limbs[] = {27753065054UL, 0UL};
static const int_t _auth_minimal_param_Bz3sqr = {{(limb_t *)_auth_minimal_param_Bz3sqr_limbs, 2, 0}};
static const limb_t _auth_minimal_param_Bz4_limbs[] = {812646UL};
static const int_t _auth_minimal_param_Bz4 = {{(limb_t *)_auth_minimal_param_Bz4_limbs, 1, 0}};
static const limb_t _auth_minimal_param_Pmodq_limbs[] = {2643201116187370UL};
static const int_t _auth_minimal_param_Pmodq = {{(limb_t *)_auth_minimal_param_Pmodq_limbs, 1, 0}};
static const limb_t _auth_minimal_param_l2Bsq0_limbs[] = {8192UL};
static const int_t _auth_minimal_param_l2Bsq0 = {{(limb_t *)_auth_minimal_param_l2Bsq0_limbs, 1, 0}};
static const limb_t _auth_minimal_param_Ppmodq_0_limbs[] = {4714705873544219UL};
static const int_t _auth_minimal_param_Ppmodq_0 = {{(limb_t *)_auth_minimal_param_Ppmodq_0_limbs, 1, 0}};
static const limb_t _auth_minimal_param_Ppmodq_1_limbs[] = {4714705869045571UL};
static const int_t _auth_minimal_param_Ppmodq_1 = {{(limb_t *)_auth_minimal_param_Ppmodq_1_limbs, 1, 0}};
static const limb_t _auth_minimal_param_Ppmodq_2_limbs[] = {4714705864850419UL};
static const int_t _auth_minimal_param_Ppmodq_2 = {{(limb_t *)_auth_minimal_param_Ppmodq_2_limbs, 1, 0}};
static const int_srcptr _auth_minimal_param_l2Bsq[] = {_auth_minimal_param_l2Bsq0};
static const int_srcptr _auth_minimal_param_Ppmodq[] = {_auth_minimal_param_Ppmodq_0, _auth_minimal_param_Ppmodq_1, _auth_minimal_param_Ppmodq_2};
static const polyring_t _auth_minimal_param_ring = {{_auth_minimal_param_q, 64, 55, 6, moduli_d64, 3, _auth_minimal_param_Pmodq, _auth_minimal_param_Ppmodq, _auth_minimal_param_inv2}};
static const dcompress_params_t _auth_minimal_param_dcomp = {{ _auth_minimal_param_q, _auth_minimal_param_qminus1, _auth_minimal_param_m, _auth_minimal_param_mby2, _auth_minimal_param_gamma, _auth_minimal_param_gammaby2, _auth_minimal_param_pow2D, _auth_minimal_param_pow2Dby2, 9, 1, 36 }};
static const abdlop_params_t _auth_minimal_param_tbox = {{ _auth_minimal_param_ring, _auth_minimal_param_dcomp, 17, 59, 0, 12, 13, _auth_minimal_param_Bsq, 1, 8, 5, 140, 1, 17, _auth_minimal_param_scM1, _auth_minimal_param_stdev1sq, 2, 12, _auth_minimal_param_scM2, _auth_minimal_param_stdev2sq}};
static const abdlop_params_t _auth_minimal_param_quad_eval_ = {{ _auth_minimal_param_ring, _auth_minimal_param_dcomp, 17, 59, 9, 3, 13, _auth_minimal_param_Bsq, 1, 8, 5, 140, 1, 17, _auth_minimal_param_scM1, _auth_minimal_param_stdev1sq, 2, 12, _auth_minimal_param_scM2, _auth_minimal_param_stdev2sq}};
static const abdlop_params_t _auth_minimal_param_quad_many_ = {{ _auth_minimal_param_ring, _auth_minimal_param_dcomp, 17, 59, 11, 1, 13, _auth_minimal_param_Bsq, 1, 8, 5, 140, 1, 17, _auth_minimal_param_scM1, _auth_minimal_param_stdev1sq, 2, 12, _auth_minimal_param_scM2, _auth_minimal_param_stdev2sq}};
static const lnp_quad_eval_params_t _auth_minimal_param_quad_eval = {{ _auth_minimal_param_quad_eval_, _auth_minimal_param_quad_many_, 4}};
static const lnp_tbox_params_t _auth_minimal_param = {{ _auth_minimal_param_tbox, _auth_minimal_param_quad_eval, 0, _auth_minimal_param_n, 4, 1, 17, 2, 12, _auth_minimal_param_scM3, _auth_minimal_param_stdev3sq, 2, 15, _auth_minimal_param_scM4, _auth_minimal_param_stdev4sq, _auth_minimal_param_Bz3sqr, _auth_minimal_param_Bz4, &_auth_minimal_param_l2Bsq[0], _auth_minimal_param_inv4, 21082UL }};

static const unsigned int auth_minimal_param_Es0[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static const unsigned int *auth_minimal_param_Es[1] = { auth_minimal_param_Es0, };
static const unsigned int auth_minimal_param_Es_nrows[1] = {16};

static const limb_t auth_minimal_param_p_limbs[] = {4294962689UL};
static const int_t auth_minimal_param_p = {{(limb_t *)auth_minimal_param_p_limbs, 1, 0}};
static const limb_t auth_minimal_param_pinv_limbs[] = {370972426753567UL};
static const int_t auth_minimal_param_pinv = {{(limb_t *)auth_minimal_param_pinv_limbs, 1, 1}};
static const unsigned int auth_minimal_param_s1_indices[4] = {0, 1, 2, 3};
static const lin_params_t auth_minimal_param = {{ _auth_minimal_param, 256, auth_minimal_param_p, auth_minimal_param_pinv, 4, auth_minimal_param_s1_indices, 4, NULL, 0,  NULL, 0, auth_minimal_param_Es, auth_minimal_param_Es_nrows, NULL, NULL }};

