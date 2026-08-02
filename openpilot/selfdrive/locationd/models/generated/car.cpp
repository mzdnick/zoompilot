#include "car.h"

namespace {
#define DIM 9
#define EDIM 9
#define MEDIM 9
typedef void (*Hfun)(double *, double *, double *);

double mass;

void set_mass(double x){ mass = x;}

double rotational_inertia;

void set_rotational_inertia(double x){ rotational_inertia = x;}

double center_to_front;

void set_center_to_front(double x){ center_to_front = x;}

double center_to_rear;

void set_center_to_rear(double x){ center_to_rear = x;}

double stiffness_front;

void set_stiffness_front(double x){ stiffness_front = x;}

double stiffness_rear;

void set_stiffness_rear(double x){ stiffness_rear = x;}
const static double MAHA_THRESH_25 = 3.8414588206941227;
const static double MAHA_THRESH_24 = 5.991464547107981;
const static double MAHA_THRESH_30 = 3.8414588206941227;
const static double MAHA_THRESH_26 = 3.8414588206941227;
const static double MAHA_THRESH_27 = 3.8414588206941227;
const static double MAHA_THRESH_29 = 3.8414588206941227;
const static double MAHA_THRESH_28 = 3.8414588206941227;
const static double MAHA_THRESH_31 = 3.8414588206941227;

/******************************************************************************
 *                      Code generated with SymPy 1.14.0                      *
 *                                                                            *
 *              See http://www.sympy.org/ for more information.               *
 *                                                                            *
 *                         This file is part of 'ekf'                         *
 ******************************************************************************/
void err_fun(double *nom_x, double *delta_x, double *out_4701800632783225768) {
   out_4701800632783225768[0] = delta_x[0] + nom_x[0];
   out_4701800632783225768[1] = delta_x[1] + nom_x[1];
   out_4701800632783225768[2] = delta_x[2] + nom_x[2];
   out_4701800632783225768[3] = delta_x[3] + nom_x[3];
   out_4701800632783225768[4] = delta_x[4] + nom_x[4];
   out_4701800632783225768[5] = delta_x[5] + nom_x[5];
   out_4701800632783225768[6] = delta_x[6] + nom_x[6];
   out_4701800632783225768[7] = delta_x[7] + nom_x[7];
   out_4701800632783225768[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_5632699205148247266) {
   out_5632699205148247266[0] = -nom_x[0] + true_x[0];
   out_5632699205148247266[1] = -nom_x[1] + true_x[1];
   out_5632699205148247266[2] = -nom_x[2] + true_x[2];
   out_5632699205148247266[3] = -nom_x[3] + true_x[3];
   out_5632699205148247266[4] = -nom_x[4] + true_x[4];
   out_5632699205148247266[5] = -nom_x[5] + true_x[5];
   out_5632699205148247266[6] = -nom_x[6] + true_x[6];
   out_5632699205148247266[7] = -nom_x[7] + true_x[7];
   out_5632699205148247266[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_8252517951555987294) {
   out_8252517951555987294[0] = 1.0;
   out_8252517951555987294[1] = 0.0;
   out_8252517951555987294[2] = 0.0;
   out_8252517951555987294[3] = 0.0;
   out_8252517951555987294[4] = 0.0;
   out_8252517951555987294[5] = 0.0;
   out_8252517951555987294[6] = 0.0;
   out_8252517951555987294[7] = 0.0;
   out_8252517951555987294[8] = 0.0;
   out_8252517951555987294[9] = 0.0;
   out_8252517951555987294[10] = 1.0;
   out_8252517951555987294[11] = 0.0;
   out_8252517951555987294[12] = 0.0;
   out_8252517951555987294[13] = 0.0;
   out_8252517951555987294[14] = 0.0;
   out_8252517951555987294[15] = 0.0;
   out_8252517951555987294[16] = 0.0;
   out_8252517951555987294[17] = 0.0;
   out_8252517951555987294[18] = 0.0;
   out_8252517951555987294[19] = 0.0;
   out_8252517951555987294[20] = 1.0;
   out_8252517951555987294[21] = 0.0;
   out_8252517951555987294[22] = 0.0;
   out_8252517951555987294[23] = 0.0;
   out_8252517951555987294[24] = 0.0;
   out_8252517951555987294[25] = 0.0;
   out_8252517951555987294[26] = 0.0;
   out_8252517951555987294[27] = 0.0;
   out_8252517951555987294[28] = 0.0;
   out_8252517951555987294[29] = 0.0;
   out_8252517951555987294[30] = 1.0;
   out_8252517951555987294[31] = 0.0;
   out_8252517951555987294[32] = 0.0;
   out_8252517951555987294[33] = 0.0;
   out_8252517951555987294[34] = 0.0;
   out_8252517951555987294[35] = 0.0;
   out_8252517951555987294[36] = 0.0;
   out_8252517951555987294[37] = 0.0;
   out_8252517951555987294[38] = 0.0;
   out_8252517951555987294[39] = 0.0;
   out_8252517951555987294[40] = 1.0;
   out_8252517951555987294[41] = 0.0;
   out_8252517951555987294[42] = 0.0;
   out_8252517951555987294[43] = 0.0;
   out_8252517951555987294[44] = 0.0;
   out_8252517951555987294[45] = 0.0;
   out_8252517951555987294[46] = 0.0;
   out_8252517951555987294[47] = 0.0;
   out_8252517951555987294[48] = 0.0;
   out_8252517951555987294[49] = 0.0;
   out_8252517951555987294[50] = 1.0;
   out_8252517951555987294[51] = 0.0;
   out_8252517951555987294[52] = 0.0;
   out_8252517951555987294[53] = 0.0;
   out_8252517951555987294[54] = 0.0;
   out_8252517951555987294[55] = 0.0;
   out_8252517951555987294[56] = 0.0;
   out_8252517951555987294[57] = 0.0;
   out_8252517951555987294[58] = 0.0;
   out_8252517951555987294[59] = 0.0;
   out_8252517951555987294[60] = 1.0;
   out_8252517951555987294[61] = 0.0;
   out_8252517951555987294[62] = 0.0;
   out_8252517951555987294[63] = 0.0;
   out_8252517951555987294[64] = 0.0;
   out_8252517951555987294[65] = 0.0;
   out_8252517951555987294[66] = 0.0;
   out_8252517951555987294[67] = 0.0;
   out_8252517951555987294[68] = 0.0;
   out_8252517951555987294[69] = 0.0;
   out_8252517951555987294[70] = 1.0;
   out_8252517951555987294[71] = 0.0;
   out_8252517951555987294[72] = 0.0;
   out_8252517951555987294[73] = 0.0;
   out_8252517951555987294[74] = 0.0;
   out_8252517951555987294[75] = 0.0;
   out_8252517951555987294[76] = 0.0;
   out_8252517951555987294[77] = 0.0;
   out_8252517951555987294[78] = 0.0;
   out_8252517951555987294[79] = 0.0;
   out_8252517951555987294[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_4184161655926891490) {
   out_4184161655926891490[0] = state[0];
   out_4184161655926891490[1] = state[1];
   out_4184161655926891490[2] = state[2];
   out_4184161655926891490[3] = state[3];
   out_4184161655926891490[4] = state[4];
   out_4184161655926891490[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8100000000000005*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_4184161655926891490[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_4184161655926891490[7] = state[7];
   out_4184161655926891490[8] = state[8];
}
void F_fun(double *state, double dt, double *out_7973072165758334077) {
   out_7973072165758334077[0] = 1;
   out_7973072165758334077[1] = 0;
   out_7973072165758334077[2] = 0;
   out_7973072165758334077[3] = 0;
   out_7973072165758334077[4] = 0;
   out_7973072165758334077[5] = 0;
   out_7973072165758334077[6] = 0;
   out_7973072165758334077[7] = 0;
   out_7973072165758334077[8] = 0;
   out_7973072165758334077[9] = 0;
   out_7973072165758334077[10] = 1;
   out_7973072165758334077[11] = 0;
   out_7973072165758334077[12] = 0;
   out_7973072165758334077[13] = 0;
   out_7973072165758334077[14] = 0;
   out_7973072165758334077[15] = 0;
   out_7973072165758334077[16] = 0;
   out_7973072165758334077[17] = 0;
   out_7973072165758334077[18] = 0;
   out_7973072165758334077[19] = 0;
   out_7973072165758334077[20] = 1;
   out_7973072165758334077[21] = 0;
   out_7973072165758334077[22] = 0;
   out_7973072165758334077[23] = 0;
   out_7973072165758334077[24] = 0;
   out_7973072165758334077[25] = 0;
   out_7973072165758334077[26] = 0;
   out_7973072165758334077[27] = 0;
   out_7973072165758334077[28] = 0;
   out_7973072165758334077[29] = 0;
   out_7973072165758334077[30] = 1;
   out_7973072165758334077[31] = 0;
   out_7973072165758334077[32] = 0;
   out_7973072165758334077[33] = 0;
   out_7973072165758334077[34] = 0;
   out_7973072165758334077[35] = 0;
   out_7973072165758334077[36] = 0;
   out_7973072165758334077[37] = 0;
   out_7973072165758334077[38] = 0;
   out_7973072165758334077[39] = 0;
   out_7973072165758334077[40] = 1;
   out_7973072165758334077[41] = 0;
   out_7973072165758334077[42] = 0;
   out_7973072165758334077[43] = 0;
   out_7973072165758334077[44] = 0;
   out_7973072165758334077[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_7973072165758334077[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_7973072165758334077[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_7973072165758334077[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_7973072165758334077[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_7973072165758334077[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_7973072165758334077[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_7973072165758334077[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_7973072165758334077[53] = -9.8100000000000005*dt;
   out_7973072165758334077[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_7973072165758334077[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_7973072165758334077[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7973072165758334077[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7973072165758334077[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_7973072165758334077[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_7973072165758334077[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_7973072165758334077[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7973072165758334077[62] = 0;
   out_7973072165758334077[63] = 0;
   out_7973072165758334077[64] = 0;
   out_7973072165758334077[65] = 0;
   out_7973072165758334077[66] = 0;
   out_7973072165758334077[67] = 0;
   out_7973072165758334077[68] = 0;
   out_7973072165758334077[69] = 0;
   out_7973072165758334077[70] = 1;
   out_7973072165758334077[71] = 0;
   out_7973072165758334077[72] = 0;
   out_7973072165758334077[73] = 0;
   out_7973072165758334077[74] = 0;
   out_7973072165758334077[75] = 0;
   out_7973072165758334077[76] = 0;
   out_7973072165758334077[77] = 0;
   out_7973072165758334077[78] = 0;
   out_7973072165758334077[79] = 0;
   out_7973072165758334077[80] = 1;
}
void h_25(double *state, double *unused, double *out_2493999060563377673) {
   out_2493999060563377673[0] = state[6];
}
void H_25(double *state, double *unused, double *out_5290534296968899482) {
   out_5290534296968899482[0] = 0;
   out_5290534296968899482[1] = 0;
   out_5290534296968899482[2] = 0;
   out_5290534296968899482[3] = 0;
   out_5290534296968899482[4] = 0;
   out_5290534296968899482[5] = 0;
   out_5290534296968899482[6] = 1;
   out_5290534296968899482[7] = 0;
   out_5290534296968899482[8] = 0;
}
void h_24(double *state, double *unused, double *out_8401558613536330381) {
   out_8401558613536330381[0] = state[4];
   out_8401558613536330381[1] = state[5];
}
void H_24(double *state, double *unused, double *out_5760991779012238206) {
   out_5760991779012238206[0] = 0;
   out_5760991779012238206[1] = 0;
   out_5760991779012238206[2] = 0;
   out_5760991779012238206[3] = 0;
   out_5760991779012238206[4] = 1;
   out_5760991779012238206[5] = 0;
   out_5760991779012238206[6] = 0;
   out_5760991779012238206[7] = 0;
   out_5760991779012238206[8] = 0;
   out_5760991779012238206[9] = 0;
   out_5760991779012238206[10] = 0;
   out_5760991779012238206[11] = 0;
   out_5760991779012238206[12] = 0;
   out_5760991779012238206[13] = 0;
   out_5760991779012238206[14] = 1;
   out_5760991779012238206[15] = 0;
   out_5760991779012238206[16] = 0;
   out_5760991779012238206[17] = 0;
}
void h_30(double *state, double *unused, double *out_7312867536427632059) {
   out_7312867536427632059[0] = state[4];
}
void H_30(double *state, double *unused, double *out_8628513446613043936) {
   out_8628513446613043936[0] = 0;
   out_8628513446613043936[1] = 0;
   out_8628513446613043936[2] = 0;
   out_8628513446613043936[3] = 0;
   out_8628513446613043936[4] = 1;
   out_8628513446613043936[5] = 0;
   out_8628513446613043936[6] = 0;
   out_8628513446613043936[7] = 0;
   out_8628513446613043936[8] = 0;
}
void h_26(double *state, double *unused, double *out_9011047971910603563) {
   out_9011047971910603563[0] = state[7];
}
void H_26(double *state, double *unused, double *out_9032037615842955706) {
   out_9032037615842955706[0] = 0;
   out_9032037615842955706[1] = 0;
   out_9032037615842955706[2] = 0;
   out_9032037615842955706[3] = 0;
   out_9032037615842955706[4] = 0;
   out_9032037615842955706[5] = 0;
   out_9032037615842955706[6] = 0;
   out_9032037615842955706[7] = 1;
   out_9032037615842955706[8] = 0;
}
void h_27(double *state, double *unused, double *out_2507674207097467972) {
   out_2507674207097467972[0] = state[3];
}
void H_27(double *state, double *unused, double *out_7594636555912564463) {
   out_7594636555912564463[0] = 0;
   out_7594636555912564463[1] = 0;
   out_7594636555912564463[2] = 0;
   out_7594636555912564463[3] = 1;
   out_7594636555912564463[4] = 0;
   out_7594636555912564463[5] = 0;
   out_7594636555912564463[6] = 0;
   out_7594636555912564463[7] = 0;
   out_7594636555912564463[8] = 0;
}
void h_29(double *state, double *unused, double *out_6369357683525969624) {
   out_6369357683525969624[0] = state[1];
}
void H_29(double *state, double *unused, double *out_9138744790927436120) {
   out_9138744790927436120[0] = 0;
   out_9138744790927436120[1] = 1;
   out_9138744790927436120[2] = 0;
   out_9138744790927436120[3] = 0;
   out_9138744790927436120[4] = 0;
   out_9138744790927436120[5] = 0;
   out_9138744790927436120[6] = 0;
   out_9138744790927436120[7] = 0;
   out_9138744790927436120[8] = 0;
}
void h_28(double *state, double *unused, double *out_304877896172385411) {
   out_304877896172385411[0] = state[0];
}
void H_28(double *state, double *unused, double *out_7344369011216789245) {
   out_7344369011216789245[0] = 1;
   out_7344369011216789245[1] = 0;
   out_7344369011216789245[2] = 0;
   out_7344369011216789245[3] = 0;
   out_7344369011216789245[4] = 0;
   out_7344369011216789245[5] = 0;
   out_7344369011216789245[6] = 0;
   out_7344369011216789245[7] = 0;
   out_7344369011216789245[8] = 0;
}
void h_31(double *state, double *unused, double *out_2769193122847883562) {
   out_2769193122847883562[0] = state[8];
}
void H_31(double *state, double *unused, double *out_5259888335091939054) {
   out_5259888335091939054[0] = 0;
   out_5259888335091939054[1] = 0;
   out_5259888335091939054[2] = 0;
   out_5259888335091939054[3] = 0;
   out_5259888335091939054[4] = 0;
   out_5259888335091939054[5] = 0;
   out_5259888335091939054[6] = 0;
   out_5259888335091939054[7] = 0;
   out_5259888335091939054[8] = 1;
}
#include <eigen3/Eigen/Dense>
#include <iostream>

typedef Eigen::Matrix<double, DIM, DIM, Eigen::RowMajor> DDM;
typedef Eigen::Matrix<double, EDIM, EDIM, Eigen::RowMajor> EEM;
typedef Eigen::Matrix<double, DIM, EDIM, Eigen::RowMajor> DEM;

void predict(double *in_x, double *in_P, double *in_Q, double dt) {
  typedef Eigen::Matrix<double, MEDIM, MEDIM, Eigen::RowMajor> RRM;

  double nx[DIM] = {0};
  double in_F[EDIM*EDIM] = {0};

  // functions from sympy
  f_fun(in_x, dt, nx);
  F_fun(in_x, dt, in_F);


  EEM F(in_F);
  EEM P(in_P);
  EEM Q(in_Q);

  RRM F_main = F.topLeftCorner(MEDIM, MEDIM);
  P.topLeftCorner(MEDIM, MEDIM) = (F_main * P.topLeftCorner(MEDIM, MEDIM)) * F_main.transpose();
  P.topRightCorner(MEDIM, EDIM - MEDIM) = F_main * P.topRightCorner(MEDIM, EDIM - MEDIM);
  P.bottomLeftCorner(EDIM - MEDIM, MEDIM) = P.bottomLeftCorner(EDIM - MEDIM, MEDIM) * F_main.transpose();

  P = P + dt*Q;

  // copy out state
  memcpy(in_x, nx, DIM * sizeof(double));
  memcpy(in_P, P.data(), EDIM * EDIM * sizeof(double));
}

// note: extra_args dim only correct when null space projecting
// otherwise 1
template <int ZDIM, int EADIM, bool MAHA_TEST>
void update(double *in_x, double *in_P, Hfun h_fun, Hfun H_fun, Hfun Hea_fun, double *in_z, double *in_R, double *in_ea, double MAHA_THRESHOLD) {
  typedef Eigen::Matrix<double, ZDIM, ZDIM, Eigen::RowMajor> ZZM;
  typedef Eigen::Matrix<double, ZDIM, DIM, Eigen::RowMajor> ZDM;
  typedef Eigen::Matrix<double, Eigen::Dynamic, EDIM, Eigen::RowMajor> XEM;
  //typedef Eigen::Matrix<double, EDIM, ZDIM, Eigen::RowMajor> EZM;
  typedef Eigen::Matrix<double, Eigen::Dynamic, 1> X1M;
  typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> XXM;

  double in_hx[ZDIM] = {0};
  double in_H[ZDIM * DIM] = {0};
  double in_H_mod[EDIM * DIM] = {0};
  double delta_x[EDIM] = {0};
  double x_new[DIM] = {0};


  // state x, P
  Eigen::Matrix<double, ZDIM, 1> z(in_z);
  EEM P(in_P);
  ZZM pre_R(in_R);

  // functions from sympy
  h_fun(in_x, in_ea, in_hx);
  H_fun(in_x, in_ea, in_H);
  ZDM pre_H(in_H);

  // get y (y = z - hx)
  Eigen::Matrix<double, ZDIM, 1> pre_y(in_hx); pre_y = z - pre_y;
  X1M y; XXM H; XXM R;
  if (Hea_fun){
    typedef Eigen::Matrix<double, ZDIM, EADIM, Eigen::RowMajor> ZAM;
    double in_Hea[ZDIM * EADIM] = {0};
    Hea_fun(in_x, in_ea, in_Hea);
    ZAM Hea(in_Hea);
    XXM A = Hea.transpose().fullPivLu().kernel();


    y = A.transpose() * pre_y;
    H = A.transpose() * pre_H;
    R = A.transpose() * pre_R * A;
  } else {
    y = pre_y;
    H = pre_H;
    R = pre_R;
  }
  // get modified H
  H_mod_fun(in_x, in_H_mod);
  DEM H_mod(in_H_mod);
  XEM H_err = H * H_mod;

  // Do mahalobis distance test
  if (MAHA_TEST){
    XXM a = (H_err * P * H_err.transpose() + R).inverse();
    double maha_dist = y.transpose() * a * y;
    if (maha_dist > MAHA_THRESHOLD){
      R = 1.0e16 * R;
    }
  }

  // Outlier resilient weighting
  double weight = 1;//(1.5)/(1 + y.squaredNorm()/R.sum());

  // kalman gains and I_KH
  XXM S = ((H_err * P) * H_err.transpose()) + R/weight;
  XEM KT = S.fullPivLu().solve(H_err * P.transpose());
  //EZM K = KT.transpose(); TODO: WHY DOES THIS NOT COMPILE?
  //EZM K = S.fullPivLu().solve(H_err * P.transpose()).transpose();
  //std::cout << "Here is the matrix rot:\n" << K << std::endl;
  EEM I_KH = Eigen::Matrix<double, EDIM, EDIM>::Identity() - (KT.transpose() * H_err);

  // update state by injecting dx
  Eigen::Matrix<double, EDIM, 1> dx(delta_x);
  dx  = (KT.transpose() * y);
  memcpy(delta_x, dx.data(), EDIM * sizeof(double));
  err_fun(in_x, delta_x, x_new);
  Eigen::Matrix<double, DIM, 1> x(x_new);

  // update cov
  P = ((I_KH * P) * I_KH.transpose()) + ((KT.transpose() * R) * KT);

  // copy out state
  memcpy(in_x, x.data(), DIM * sizeof(double));
  memcpy(in_P, P.data(), EDIM * EDIM * sizeof(double));
  memcpy(in_z, y.data(), y.rows() * sizeof(double));
}




}
extern "C" {

void car_update_25(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_25, H_25, NULL, in_z, in_R, in_ea, MAHA_THRESH_25);
}
void car_update_24(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<2, 3, 0>(in_x, in_P, h_24, H_24, NULL, in_z, in_R, in_ea, MAHA_THRESH_24);
}
void car_update_30(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_30, H_30, NULL, in_z, in_R, in_ea, MAHA_THRESH_30);
}
void car_update_26(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_26, H_26, NULL, in_z, in_R, in_ea, MAHA_THRESH_26);
}
void car_update_27(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_27, H_27, NULL, in_z, in_R, in_ea, MAHA_THRESH_27);
}
void car_update_29(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_29, H_29, NULL, in_z, in_R, in_ea, MAHA_THRESH_29);
}
void car_update_28(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_28, H_28, NULL, in_z, in_R, in_ea, MAHA_THRESH_28);
}
void car_update_31(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_31, H_31, NULL, in_z, in_R, in_ea, MAHA_THRESH_31);
}
void car_err_fun(double *nom_x, double *delta_x, double *out_4701800632783225768) {
  err_fun(nom_x, delta_x, out_4701800632783225768);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_5632699205148247266) {
  inv_err_fun(nom_x, true_x, out_5632699205148247266);
}
void car_H_mod_fun(double *state, double *out_8252517951555987294) {
  H_mod_fun(state, out_8252517951555987294);
}
void car_f_fun(double *state, double dt, double *out_4184161655926891490) {
  f_fun(state,  dt, out_4184161655926891490);
}
void car_F_fun(double *state, double dt, double *out_7973072165758334077) {
  F_fun(state,  dt, out_7973072165758334077);
}
void car_h_25(double *state, double *unused, double *out_2493999060563377673) {
  h_25(state, unused, out_2493999060563377673);
}
void car_H_25(double *state, double *unused, double *out_5290534296968899482) {
  H_25(state, unused, out_5290534296968899482);
}
void car_h_24(double *state, double *unused, double *out_8401558613536330381) {
  h_24(state, unused, out_8401558613536330381);
}
void car_H_24(double *state, double *unused, double *out_5760991779012238206) {
  H_24(state, unused, out_5760991779012238206);
}
void car_h_30(double *state, double *unused, double *out_7312867536427632059) {
  h_30(state, unused, out_7312867536427632059);
}
void car_H_30(double *state, double *unused, double *out_8628513446613043936) {
  H_30(state, unused, out_8628513446613043936);
}
void car_h_26(double *state, double *unused, double *out_9011047971910603563) {
  h_26(state, unused, out_9011047971910603563);
}
void car_H_26(double *state, double *unused, double *out_9032037615842955706) {
  H_26(state, unused, out_9032037615842955706);
}
void car_h_27(double *state, double *unused, double *out_2507674207097467972) {
  h_27(state, unused, out_2507674207097467972);
}
void car_H_27(double *state, double *unused, double *out_7594636555912564463) {
  H_27(state, unused, out_7594636555912564463);
}
void car_h_29(double *state, double *unused, double *out_6369357683525969624) {
  h_29(state, unused, out_6369357683525969624);
}
void car_H_29(double *state, double *unused, double *out_9138744790927436120) {
  H_29(state, unused, out_9138744790927436120);
}
void car_h_28(double *state, double *unused, double *out_304877896172385411) {
  h_28(state, unused, out_304877896172385411);
}
void car_H_28(double *state, double *unused, double *out_7344369011216789245) {
  H_28(state, unused, out_7344369011216789245);
}
void car_h_31(double *state, double *unused, double *out_2769193122847883562) {
  h_31(state, unused, out_2769193122847883562);
}
void car_H_31(double *state, double *unused, double *out_5259888335091939054) {
  H_31(state, unused, out_5259888335091939054);
}
void car_predict(double *in_x, double *in_P, double *in_Q, double dt) {
  predict(in_x, in_P, in_Q, dt);
}
void car_set_mass(double x) {
  set_mass(x);
}
void car_set_rotational_inertia(double x) {
  set_rotational_inertia(x);
}
void car_set_center_to_front(double x) {
  set_center_to_front(x);
}
void car_set_center_to_rear(double x) {
  set_center_to_rear(x);
}
void car_set_stiffness_front(double x) {
  set_stiffness_front(x);
}
void car_set_stiffness_rear(double x) {
  set_stiffness_rear(x);
}
}

const EKF car = {
  .name = "car",
  .kinds = { 25, 24, 30, 26, 27, 29, 28, 31 },
  .feature_kinds = {  },
  .f_fun = car_f_fun,
  .F_fun = car_F_fun,
  .err_fun = car_err_fun,
  .inv_err_fun = car_inv_err_fun,
  .H_mod_fun = car_H_mod_fun,
  .predict = car_predict,
  .hs = {
    { 25, car_h_25 },
    { 24, car_h_24 },
    { 30, car_h_30 },
    { 26, car_h_26 },
    { 27, car_h_27 },
    { 29, car_h_29 },
    { 28, car_h_28 },
    { 31, car_h_31 },
  },
  .Hs = {
    { 25, car_H_25 },
    { 24, car_H_24 },
    { 30, car_H_30 },
    { 26, car_H_26 },
    { 27, car_H_27 },
    { 29, car_H_29 },
    { 28, car_H_28 },
    { 31, car_H_31 },
  },
  .updates = {
    { 25, car_update_25 },
    { 24, car_update_24 },
    { 30, car_update_30 },
    { 26, car_update_26 },
    { 27, car_update_27 },
    { 29, car_update_29 },
    { 28, car_update_28 },
    { 31, car_update_31 },
  },
  .Hes = {
  },
  .sets = {
    { "mass", car_set_mass },
    { "rotational_inertia", car_set_rotational_inertia },
    { "center_to_front", car_set_center_to_front },
    { "center_to_rear", car_set_center_to_rear },
    { "stiffness_front", car_set_stiffness_front },
    { "stiffness_rear", car_set_stiffness_rear },
  },
  .extra_routines = {
  },
};

ekf_lib_init(car)
