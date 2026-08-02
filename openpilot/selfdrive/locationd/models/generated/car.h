#pragma once
#include "rednose/helpers/ekf.h"
extern "C" {
void car_update_25(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_24(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_30(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_26(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_27(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_29(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_28(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_31(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_err_fun(double *nom_x, double *delta_x, double *out_4701800632783225768);
void car_inv_err_fun(double *nom_x, double *true_x, double *out_5632699205148247266);
void car_H_mod_fun(double *state, double *out_8252517951555987294);
void car_f_fun(double *state, double dt, double *out_4184161655926891490);
void car_F_fun(double *state, double dt, double *out_7973072165758334077);
void car_h_25(double *state, double *unused, double *out_2493999060563377673);
void car_H_25(double *state, double *unused, double *out_5290534296968899482);
void car_h_24(double *state, double *unused, double *out_8401558613536330381);
void car_H_24(double *state, double *unused, double *out_5760991779012238206);
void car_h_30(double *state, double *unused, double *out_7312867536427632059);
void car_H_30(double *state, double *unused, double *out_8628513446613043936);
void car_h_26(double *state, double *unused, double *out_9011047971910603563);
void car_H_26(double *state, double *unused, double *out_9032037615842955706);
void car_h_27(double *state, double *unused, double *out_2507674207097467972);
void car_H_27(double *state, double *unused, double *out_7594636555912564463);
void car_h_29(double *state, double *unused, double *out_6369357683525969624);
void car_H_29(double *state, double *unused, double *out_9138744790927436120);
void car_h_28(double *state, double *unused, double *out_304877896172385411);
void car_H_28(double *state, double *unused, double *out_7344369011216789245);
void car_h_31(double *state, double *unused, double *out_2769193122847883562);
void car_H_31(double *state, double *unused, double *out_5259888335091939054);
void car_predict(double *in_x, double *in_P, double *in_Q, double dt);
void car_set_mass(double x);
void car_set_rotational_inertia(double x);
void car_set_center_to_front(double x);
void car_set_center_to_rear(double x);
void car_set_stiffness_front(double x);
void car_set_stiffness_rear(double x);
}