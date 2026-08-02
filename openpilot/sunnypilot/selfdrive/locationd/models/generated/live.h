#pragma once
#include "rednose/helpers/ekf.h"
extern "C" {
void live_update_4(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_9(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_10(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_12(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_35(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_32(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_13(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_14(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_33(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_H(double *in_vec, double *out_5191804246572640259);
void live_err_fun(double *nom_x, double *delta_x, double *out_1587297889195541092);
void live_inv_err_fun(double *nom_x, double *true_x, double *out_5930905385372611075);
void live_H_mod_fun(double *state, double *out_721668377147939690);
void live_f_fun(double *state, double dt, double *out_242224780108512892);
void live_F_fun(double *state, double dt, double *out_4826770192310663723);
void live_h_4(double *state, double *unused, double *out_4520806810906419003);
void live_H_4(double *state, double *unused, double *out_1765819237338500838);
void live_h_9(double *state, double *unused, double *out_206906829334474191);
void live_H_9(double *state, double *unused, double *out_4654680789618580180);
void live_h_10(double *state, double *unused, double *out_5847870081838852500);
void live_H_10(double *state, double *unused, double *out_6096531847834660134);
void live_h_12(double *state, double *unused, double *out_8017875880567848471);
void live_H_12(double *state, double *unused, double *out_9013796522688600286);
void live_h_35(double *state, double *unused, double *out_4270935571548435451);
void live_H_35(double *state, double *unused, double *out_5132481294711108214);
void live_h_32(double *state, double *unused, double *out_521242455236504755);
void live_H_32(double *state, double *unused, double *out_7837505807612858151);
void live_h_13(double *state, double *unused, double *out_1707760309290888499);
void live_H_13(double *state, double *unused, double *out_5832176588590625076);
void live_h_14(double *state, double *unused, double *out_206906829334474191);
void live_H_14(double *state, double *unused, double *out_4654680789618580180);
void live_h_33(double *state, double *unused, double *out_2617417254616243080);
void live_H_33(double *state, double *unused, double *out_3117676485724728973);
void live_predict(double *in_x, double *in_P, double *in_Q, double dt);
}