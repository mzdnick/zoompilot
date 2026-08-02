#include "pose.h"

namespace {
#define DIM 18
#define EDIM 18
#define MEDIM 18
typedef void (*Hfun)(double *, double *, double *);
const static double MAHA_THRESH_4 = 7.814727903251177;
const static double MAHA_THRESH_10 = 7.814727903251177;
const static double MAHA_THRESH_13 = 7.814727903251177;
const static double MAHA_THRESH_14 = 7.814727903251177;

/******************************************************************************
 *                      Code generated with SymPy 1.14.0                      *
 *                                                                            *
 *              See http://www.sympy.org/ for more information.               *
 *                                                                            *
 *                         This file is part of 'ekf'                         *
 ******************************************************************************/
void err_fun(double *nom_x, double *delta_x, double *out_2294198930716030722) {
   out_2294198930716030722[0] = delta_x[0] + nom_x[0];
   out_2294198930716030722[1] = delta_x[1] + nom_x[1];
   out_2294198930716030722[2] = delta_x[2] + nom_x[2];
   out_2294198930716030722[3] = delta_x[3] + nom_x[3];
   out_2294198930716030722[4] = delta_x[4] + nom_x[4];
   out_2294198930716030722[5] = delta_x[5] + nom_x[5];
   out_2294198930716030722[6] = delta_x[6] + nom_x[6];
   out_2294198930716030722[7] = delta_x[7] + nom_x[7];
   out_2294198930716030722[8] = delta_x[8] + nom_x[8];
   out_2294198930716030722[9] = delta_x[9] + nom_x[9];
   out_2294198930716030722[10] = delta_x[10] + nom_x[10];
   out_2294198930716030722[11] = delta_x[11] + nom_x[11];
   out_2294198930716030722[12] = delta_x[12] + nom_x[12];
   out_2294198930716030722[13] = delta_x[13] + nom_x[13];
   out_2294198930716030722[14] = delta_x[14] + nom_x[14];
   out_2294198930716030722[15] = delta_x[15] + nom_x[15];
   out_2294198930716030722[16] = delta_x[16] + nom_x[16];
   out_2294198930716030722[17] = delta_x[17] + nom_x[17];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_4974602922426709952) {
   out_4974602922426709952[0] = -nom_x[0] + true_x[0];
   out_4974602922426709952[1] = -nom_x[1] + true_x[1];
   out_4974602922426709952[2] = -nom_x[2] + true_x[2];
   out_4974602922426709952[3] = -nom_x[3] + true_x[3];
   out_4974602922426709952[4] = -nom_x[4] + true_x[4];
   out_4974602922426709952[5] = -nom_x[5] + true_x[5];
   out_4974602922426709952[6] = -nom_x[6] + true_x[6];
   out_4974602922426709952[7] = -nom_x[7] + true_x[7];
   out_4974602922426709952[8] = -nom_x[8] + true_x[8];
   out_4974602922426709952[9] = -nom_x[9] + true_x[9];
   out_4974602922426709952[10] = -nom_x[10] + true_x[10];
   out_4974602922426709952[11] = -nom_x[11] + true_x[11];
   out_4974602922426709952[12] = -nom_x[12] + true_x[12];
   out_4974602922426709952[13] = -nom_x[13] + true_x[13];
   out_4974602922426709952[14] = -nom_x[14] + true_x[14];
   out_4974602922426709952[15] = -nom_x[15] + true_x[15];
   out_4974602922426709952[16] = -nom_x[16] + true_x[16];
   out_4974602922426709952[17] = -nom_x[17] + true_x[17];
}
void H_mod_fun(double *state, double *out_4759570850015884975) {
   out_4759570850015884975[0] = 1.0;
   out_4759570850015884975[1] = 0.0;
   out_4759570850015884975[2] = 0.0;
   out_4759570850015884975[3] = 0.0;
   out_4759570850015884975[4] = 0.0;
   out_4759570850015884975[5] = 0.0;
   out_4759570850015884975[6] = 0.0;
   out_4759570850015884975[7] = 0.0;
   out_4759570850015884975[8] = 0.0;
   out_4759570850015884975[9] = 0.0;
   out_4759570850015884975[10] = 0.0;
   out_4759570850015884975[11] = 0.0;
   out_4759570850015884975[12] = 0.0;
   out_4759570850015884975[13] = 0.0;
   out_4759570850015884975[14] = 0.0;
   out_4759570850015884975[15] = 0.0;
   out_4759570850015884975[16] = 0.0;
   out_4759570850015884975[17] = 0.0;
   out_4759570850015884975[18] = 0.0;
   out_4759570850015884975[19] = 1.0;
   out_4759570850015884975[20] = 0.0;
   out_4759570850015884975[21] = 0.0;
   out_4759570850015884975[22] = 0.0;
   out_4759570850015884975[23] = 0.0;
   out_4759570850015884975[24] = 0.0;
   out_4759570850015884975[25] = 0.0;
   out_4759570850015884975[26] = 0.0;
   out_4759570850015884975[27] = 0.0;
   out_4759570850015884975[28] = 0.0;
   out_4759570850015884975[29] = 0.0;
   out_4759570850015884975[30] = 0.0;
   out_4759570850015884975[31] = 0.0;
   out_4759570850015884975[32] = 0.0;
   out_4759570850015884975[33] = 0.0;
   out_4759570850015884975[34] = 0.0;
   out_4759570850015884975[35] = 0.0;
   out_4759570850015884975[36] = 0.0;
   out_4759570850015884975[37] = 0.0;
   out_4759570850015884975[38] = 1.0;
   out_4759570850015884975[39] = 0.0;
   out_4759570850015884975[40] = 0.0;
   out_4759570850015884975[41] = 0.0;
   out_4759570850015884975[42] = 0.0;
   out_4759570850015884975[43] = 0.0;
   out_4759570850015884975[44] = 0.0;
   out_4759570850015884975[45] = 0.0;
   out_4759570850015884975[46] = 0.0;
   out_4759570850015884975[47] = 0.0;
   out_4759570850015884975[48] = 0.0;
   out_4759570850015884975[49] = 0.0;
   out_4759570850015884975[50] = 0.0;
   out_4759570850015884975[51] = 0.0;
   out_4759570850015884975[52] = 0.0;
   out_4759570850015884975[53] = 0.0;
   out_4759570850015884975[54] = 0.0;
   out_4759570850015884975[55] = 0.0;
   out_4759570850015884975[56] = 0.0;
   out_4759570850015884975[57] = 1.0;
   out_4759570850015884975[58] = 0.0;
   out_4759570850015884975[59] = 0.0;
   out_4759570850015884975[60] = 0.0;
   out_4759570850015884975[61] = 0.0;
   out_4759570850015884975[62] = 0.0;
   out_4759570850015884975[63] = 0.0;
   out_4759570850015884975[64] = 0.0;
   out_4759570850015884975[65] = 0.0;
   out_4759570850015884975[66] = 0.0;
   out_4759570850015884975[67] = 0.0;
   out_4759570850015884975[68] = 0.0;
   out_4759570850015884975[69] = 0.0;
   out_4759570850015884975[70] = 0.0;
   out_4759570850015884975[71] = 0.0;
   out_4759570850015884975[72] = 0.0;
   out_4759570850015884975[73] = 0.0;
   out_4759570850015884975[74] = 0.0;
   out_4759570850015884975[75] = 0.0;
   out_4759570850015884975[76] = 1.0;
   out_4759570850015884975[77] = 0.0;
   out_4759570850015884975[78] = 0.0;
   out_4759570850015884975[79] = 0.0;
   out_4759570850015884975[80] = 0.0;
   out_4759570850015884975[81] = 0.0;
   out_4759570850015884975[82] = 0.0;
   out_4759570850015884975[83] = 0.0;
   out_4759570850015884975[84] = 0.0;
   out_4759570850015884975[85] = 0.0;
   out_4759570850015884975[86] = 0.0;
   out_4759570850015884975[87] = 0.0;
   out_4759570850015884975[88] = 0.0;
   out_4759570850015884975[89] = 0.0;
   out_4759570850015884975[90] = 0.0;
   out_4759570850015884975[91] = 0.0;
   out_4759570850015884975[92] = 0.0;
   out_4759570850015884975[93] = 0.0;
   out_4759570850015884975[94] = 0.0;
   out_4759570850015884975[95] = 1.0;
   out_4759570850015884975[96] = 0.0;
   out_4759570850015884975[97] = 0.0;
   out_4759570850015884975[98] = 0.0;
   out_4759570850015884975[99] = 0.0;
   out_4759570850015884975[100] = 0.0;
   out_4759570850015884975[101] = 0.0;
   out_4759570850015884975[102] = 0.0;
   out_4759570850015884975[103] = 0.0;
   out_4759570850015884975[104] = 0.0;
   out_4759570850015884975[105] = 0.0;
   out_4759570850015884975[106] = 0.0;
   out_4759570850015884975[107] = 0.0;
   out_4759570850015884975[108] = 0.0;
   out_4759570850015884975[109] = 0.0;
   out_4759570850015884975[110] = 0.0;
   out_4759570850015884975[111] = 0.0;
   out_4759570850015884975[112] = 0.0;
   out_4759570850015884975[113] = 0.0;
   out_4759570850015884975[114] = 1.0;
   out_4759570850015884975[115] = 0.0;
   out_4759570850015884975[116] = 0.0;
   out_4759570850015884975[117] = 0.0;
   out_4759570850015884975[118] = 0.0;
   out_4759570850015884975[119] = 0.0;
   out_4759570850015884975[120] = 0.0;
   out_4759570850015884975[121] = 0.0;
   out_4759570850015884975[122] = 0.0;
   out_4759570850015884975[123] = 0.0;
   out_4759570850015884975[124] = 0.0;
   out_4759570850015884975[125] = 0.0;
   out_4759570850015884975[126] = 0.0;
   out_4759570850015884975[127] = 0.0;
   out_4759570850015884975[128] = 0.0;
   out_4759570850015884975[129] = 0.0;
   out_4759570850015884975[130] = 0.0;
   out_4759570850015884975[131] = 0.0;
   out_4759570850015884975[132] = 0.0;
   out_4759570850015884975[133] = 1.0;
   out_4759570850015884975[134] = 0.0;
   out_4759570850015884975[135] = 0.0;
   out_4759570850015884975[136] = 0.0;
   out_4759570850015884975[137] = 0.0;
   out_4759570850015884975[138] = 0.0;
   out_4759570850015884975[139] = 0.0;
   out_4759570850015884975[140] = 0.0;
   out_4759570850015884975[141] = 0.0;
   out_4759570850015884975[142] = 0.0;
   out_4759570850015884975[143] = 0.0;
   out_4759570850015884975[144] = 0.0;
   out_4759570850015884975[145] = 0.0;
   out_4759570850015884975[146] = 0.0;
   out_4759570850015884975[147] = 0.0;
   out_4759570850015884975[148] = 0.0;
   out_4759570850015884975[149] = 0.0;
   out_4759570850015884975[150] = 0.0;
   out_4759570850015884975[151] = 0.0;
   out_4759570850015884975[152] = 1.0;
   out_4759570850015884975[153] = 0.0;
   out_4759570850015884975[154] = 0.0;
   out_4759570850015884975[155] = 0.0;
   out_4759570850015884975[156] = 0.0;
   out_4759570850015884975[157] = 0.0;
   out_4759570850015884975[158] = 0.0;
   out_4759570850015884975[159] = 0.0;
   out_4759570850015884975[160] = 0.0;
   out_4759570850015884975[161] = 0.0;
   out_4759570850015884975[162] = 0.0;
   out_4759570850015884975[163] = 0.0;
   out_4759570850015884975[164] = 0.0;
   out_4759570850015884975[165] = 0.0;
   out_4759570850015884975[166] = 0.0;
   out_4759570850015884975[167] = 0.0;
   out_4759570850015884975[168] = 0.0;
   out_4759570850015884975[169] = 0.0;
   out_4759570850015884975[170] = 0.0;
   out_4759570850015884975[171] = 1.0;
   out_4759570850015884975[172] = 0.0;
   out_4759570850015884975[173] = 0.0;
   out_4759570850015884975[174] = 0.0;
   out_4759570850015884975[175] = 0.0;
   out_4759570850015884975[176] = 0.0;
   out_4759570850015884975[177] = 0.0;
   out_4759570850015884975[178] = 0.0;
   out_4759570850015884975[179] = 0.0;
   out_4759570850015884975[180] = 0.0;
   out_4759570850015884975[181] = 0.0;
   out_4759570850015884975[182] = 0.0;
   out_4759570850015884975[183] = 0.0;
   out_4759570850015884975[184] = 0.0;
   out_4759570850015884975[185] = 0.0;
   out_4759570850015884975[186] = 0.0;
   out_4759570850015884975[187] = 0.0;
   out_4759570850015884975[188] = 0.0;
   out_4759570850015884975[189] = 0.0;
   out_4759570850015884975[190] = 1.0;
   out_4759570850015884975[191] = 0.0;
   out_4759570850015884975[192] = 0.0;
   out_4759570850015884975[193] = 0.0;
   out_4759570850015884975[194] = 0.0;
   out_4759570850015884975[195] = 0.0;
   out_4759570850015884975[196] = 0.0;
   out_4759570850015884975[197] = 0.0;
   out_4759570850015884975[198] = 0.0;
   out_4759570850015884975[199] = 0.0;
   out_4759570850015884975[200] = 0.0;
   out_4759570850015884975[201] = 0.0;
   out_4759570850015884975[202] = 0.0;
   out_4759570850015884975[203] = 0.0;
   out_4759570850015884975[204] = 0.0;
   out_4759570850015884975[205] = 0.0;
   out_4759570850015884975[206] = 0.0;
   out_4759570850015884975[207] = 0.0;
   out_4759570850015884975[208] = 0.0;
   out_4759570850015884975[209] = 1.0;
   out_4759570850015884975[210] = 0.0;
   out_4759570850015884975[211] = 0.0;
   out_4759570850015884975[212] = 0.0;
   out_4759570850015884975[213] = 0.0;
   out_4759570850015884975[214] = 0.0;
   out_4759570850015884975[215] = 0.0;
   out_4759570850015884975[216] = 0.0;
   out_4759570850015884975[217] = 0.0;
   out_4759570850015884975[218] = 0.0;
   out_4759570850015884975[219] = 0.0;
   out_4759570850015884975[220] = 0.0;
   out_4759570850015884975[221] = 0.0;
   out_4759570850015884975[222] = 0.0;
   out_4759570850015884975[223] = 0.0;
   out_4759570850015884975[224] = 0.0;
   out_4759570850015884975[225] = 0.0;
   out_4759570850015884975[226] = 0.0;
   out_4759570850015884975[227] = 0.0;
   out_4759570850015884975[228] = 1.0;
   out_4759570850015884975[229] = 0.0;
   out_4759570850015884975[230] = 0.0;
   out_4759570850015884975[231] = 0.0;
   out_4759570850015884975[232] = 0.0;
   out_4759570850015884975[233] = 0.0;
   out_4759570850015884975[234] = 0.0;
   out_4759570850015884975[235] = 0.0;
   out_4759570850015884975[236] = 0.0;
   out_4759570850015884975[237] = 0.0;
   out_4759570850015884975[238] = 0.0;
   out_4759570850015884975[239] = 0.0;
   out_4759570850015884975[240] = 0.0;
   out_4759570850015884975[241] = 0.0;
   out_4759570850015884975[242] = 0.0;
   out_4759570850015884975[243] = 0.0;
   out_4759570850015884975[244] = 0.0;
   out_4759570850015884975[245] = 0.0;
   out_4759570850015884975[246] = 0.0;
   out_4759570850015884975[247] = 1.0;
   out_4759570850015884975[248] = 0.0;
   out_4759570850015884975[249] = 0.0;
   out_4759570850015884975[250] = 0.0;
   out_4759570850015884975[251] = 0.0;
   out_4759570850015884975[252] = 0.0;
   out_4759570850015884975[253] = 0.0;
   out_4759570850015884975[254] = 0.0;
   out_4759570850015884975[255] = 0.0;
   out_4759570850015884975[256] = 0.0;
   out_4759570850015884975[257] = 0.0;
   out_4759570850015884975[258] = 0.0;
   out_4759570850015884975[259] = 0.0;
   out_4759570850015884975[260] = 0.0;
   out_4759570850015884975[261] = 0.0;
   out_4759570850015884975[262] = 0.0;
   out_4759570850015884975[263] = 0.0;
   out_4759570850015884975[264] = 0.0;
   out_4759570850015884975[265] = 0.0;
   out_4759570850015884975[266] = 1.0;
   out_4759570850015884975[267] = 0.0;
   out_4759570850015884975[268] = 0.0;
   out_4759570850015884975[269] = 0.0;
   out_4759570850015884975[270] = 0.0;
   out_4759570850015884975[271] = 0.0;
   out_4759570850015884975[272] = 0.0;
   out_4759570850015884975[273] = 0.0;
   out_4759570850015884975[274] = 0.0;
   out_4759570850015884975[275] = 0.0;
   out_4759570850015884975[276] = 0.0;
   out_4759570850015884975[277] = 0.0;
   out_4759570850015884975[278] = 0.0;
   out_4759570850015884975[279] = 0.0;
   out_4759570850015884975[280] = 0.0;
   out_4759570850015884975[281] = 0.0;
   out_4759570850015884975[282] = 0.0;
   out_4759570850015884975[283] = 0.0;
   out_4759570850015884975[284] = 0.0;
   out_4759570850015884975[285] = 1.0;
   out_4759570850015884975[286] = 0.0;
   out_4759570850015884975[287] = 0.0;
   out_4759570850015884975[288] = 0.0;
   out_4759570850015884975[289] = 0.0;
   out_4759570850015884975[290] = 0.0;
   out_4759570850015884975[291] = 0.0;
   out_4759570850015884975[292] = 0.0;
   out_4759570850015884975[293] = 0.0;
   out_4759570850015884975[294] = 0.0;
   out_4759570850015884975[295] = 0.0;
   out_4759570850015884975[296] = 0.0;
   out_4759570850015884975[297] = 0.0;
   out_4759570850015884975[298] = 0.0;
   out_4759570850015884975[299] = 0.0;
   out_4759570850015884975[300] = 0.0;
   out_4759570850015884975[301] = 0.0;
   out_4759570850015884975[302] = 0.0;
   out_4759570850015884975[303] = 0.0;
   out_4759570850015884975[304] = 1.0;
   out_4759570850015884975[305] = 0.0;
   out_4759570850015884975[306] = 0.0;
   out_4759570850015884975[307] = 0.0;
   out_4759570850015884975[308] = 0.0;
   out_4759570850015884975[309] = 0.0;
   out_4759570850015884975[310] = 0.0;
   out_4759570850015884975[311] = 0.0;
   out_4759570850015884975[312] = 0.0;
   out_4759570850015884975[313] = 0.0;
   out_4759570850015884975[314] = 0.0;
   out_4759570850015884975[315] = 0.0;
   out_4759570850015884975[316] = 0.0;
   out_4759570850015884975[317] = 0.0;
   out_4759570850015884975[318] = 0.0;
   out_4759570850015884975[319] = 0.0;
   out_4759570850015884975[320] = 0.0;
   out_4759570850015884975[321] = 0.0;
   out_4759570850015884975[322] = 0.0;
   out_4759570850015884975[323] = 1.0;
}
void f_fun(double *state, double dt, double *out_1340898844102295762) {
   out_1340898844102295762[0] = atan2((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), -(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]));
   out_1340898844102295762[1] = asin(sin(dt*state[7])*cos(state[0])*cos(state[1]) - sin(dt*state[8])*sin(state[0])*cos(dt*state[7])*cos(state[1]) + sin(state[1])*cos(dt*state[7])*cos(dt*state[8]));
   out_1340898844102295762[2] = atan2(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), -(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]));
   out_1340898844102295762[3] = dt*state[12] + state[3];
   out_1340898844102295762[4] = dt*state[13] + state[4];
   out_1340898844102295762[5] = dt*state[14] + state[5];
   out_1340898844102295762[6] = state[6];
   out_1340898844102295762[7] = state[7];
   out_1340898844102295762[8] = state[8];
   out_1340898844102295762[9] = state[9];
   out_1340898844102295762[10] = state[10];
   out_1340898844102295762[11] = state[11];
   out_1340898844102295762[12] = state[12];
   out_1340898844102295762[13] = state[13];
   out_1340898844102295762[14] = state[14];
   out_1340898844102295762[15] = state[15];
   out_1340898844102295762[16] = state[16];
   out_1340898844102295762[17] = state[17];
}
void F_fun(double *state, double dt, double *out_6444369143388310098) {
   out_6444369143388310098[0] = ((-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*cos(state[0])*cos(state[1]) - sin(state[0])*cos(dt*state[6])*cos(dt*state[7])*cos(state[1]))*(-(sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) + (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) - sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2)) + ((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*cos(state[0])*cos(state[1]) - sin(dt*state[6])*sin(state[0])*cos(dt*state[7])*cos(state[1]))*(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2));
   out_6444369143388310098[1] = ((-sin(dt*state[6])*sin(dt*state[8]) - sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*cos(state[1]) - (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*sin(state[1]) - sin(state[1])*cos(dt*state[6])*cos(dt*state[7])*cos(state[0]))*(-(sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) + (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) - sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2)) + (-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))*(-(sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*sin(state[1]) + (-sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) + sin(dt*state[8])*cos(dt*state[6]))*cos(state[1]) - sin(dt*state[6])*sin(state[1])*cos(dt*state[7])*cos(state[0]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2));
   out_6444369143388310098[2] = 0;
   out_6444369143388310098[3] = 0;
   out_6444369143388310098[4] = 0;
   out_6444369143388310098[5] = 0;
   out_6444369143388310098[6] = (-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))*(dt*cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]) + (-dt*sin(dt*state[6])*sin(dt*state[8]) - dt*sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-dt*sin(dt*state[6])*cos(dt*state[8]) + dt*sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2)) + (-(sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) + (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) - sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))*(-dt*sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]) + (-dt*sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) - dt*cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) + (dt*sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - dt*sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2));
   out_6444369143388310098[7] = (-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))*(-dt*sin(dt*state[6])*sin(dt*state[7])*cos(state[0])*cos(state[1]) + dt*sin(dt*state[6])*sin(dt*state[8])*sin(state[0])*cos(dt*state[7])*cos(state[1]) - dt*sin(dt*state[6])*sin(state[1])*cos(dt*state[7])*cos(dt*state[8]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2)) + (-(sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) + (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) - sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))*(-dt*sin(dt*state[7])*cos(dt*state[6])*cos(state[0])*cos(state[1]) + dt*sin(dt*state[8])*sin(state[0])*cos(dt*state[6])*cos(dt*state[7])*cos(state[1]) - dt*sin(state[1])*cos(dt*state[6])*cos(dt*state[7])*cos(dt*state[8]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2));
   out_6444369143388310098[8] = ((dt*sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + dt*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (dt*sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - dt*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]))*(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2)) + ((dt*sin(dt*state[6])*sin(dt*state[8]) + dt*sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) + (-dt*sin(dt*state[6])*cos(dt*state[8]) + dt*sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]))*(-(sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) + (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) - sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]))/(pow(-(sin(dt*state[6])*sin(dt*state[8]) + sin(dt*state[7])*cos(dt*state[6])*cos(dt*state[8]))*sin(state[1]) + (-sin(dt*state[6])*cos(dt*state[8]) + sin(dt*state[7])*sin(dt*state[8])*cos(dt*state[6]))*sin(state[0])*cos(state[1]) + cos(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2) + pow((sin(dt*state[6])*sin(dt*state[7])*sin(dt*state[8]) + cos(dt*state[6])*cos(dt*state[8]))*sin(state[0])*cos(state[1]) - (sin(dt*state[6])*sin(dt*state[7])*cos(dt*state[8]) - sin(dt*state[8])*cos(dt*state[6]))*sin(state[1]) + sin(dt*state[6])*cos(dt*state[7])*cos(state[0])*cos(state[1]), 2));
   out_6444369143388310098[9] = 0;
   out_6444369143388310098[10] = 0;
   out_6444369143388310098[11] = 0;
   out_6444369143388310098[12] = 0;
   out_6444369143388310098[13] = 0;
   out_6444369143388310098[14] = 0;
   out_6444369143388310098[15] = 0;
   out_6444369143388310098[16] = 0;
   out_6444369143388310098[17] = 0;
   out_6444369143388310098[18] = (-sin(dt*state[7])*sin(state[0])*cos(state[1]) - sin(dt*state[8])*cos(dt*state[7])*cos(state[0])*cos(state[1]))/sqrt(1 - pow(sin(dt*state[7])*cos(state[0])*cos(state[1]) - sin(dt*state[8])*sin(state[0])*cos(dt*state[7])*cos(state[1]) + sin(state[1])*cos(dt*state[7])*cos(dt*state[8]), 2));
   out_6444369143388310098[19] = (-sin(dt*state[7])*sin(state[1])*cos(state[0]) + sin(dt*state[8])*sin(state[0])*sin(state[1])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1]))/sqrt(1 - pow(sin(dt*state[7])*cos(state[0])*cos(state[1]) - sin(dt*state[8])*sin(state[0])*cos(dt*state[7])*cos(state[1]) + sin(state[1])*cos(dt*state[7])*cos(dt*state[8]), 2));
   out_6444369143388310098[20] = 0;
   out_6444369143388310098[21] = 0;
   out_6444369143388310098[22] = 0;
   out_6444369143388310098[23] = 0;
   out_6444369143388310098[24] = 0;
   out_6444369143388310098[25] = (dt*sin(dt*state[7])*sin(dt*state[8])*sin(state[0])*cos(state[1]) - dt*sin(dt*state[7])*sin(state[1])*cos(dt*state[8]) + dt*cos(dt*state[7])*cos(state[0])*cos(state[1]))/sqrt(1 - pow(sin(dt*state[7])*cos(state[0])*cos(state[1]) - sin(dt*state[8])*sin(state[0])*cos(dt*state[7])*cos(state[1]) + sin(state[1])*cos(dt*state[7])*cos(dt*state[8]), 2));
   out_6444369143388310098[26] = (-dt*sin(dt*state[8])*sin(state[1])*cos(dt*state[7]) - dt*sin(state[0])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]))/sqrt(1 - pow(sin(dt*state[7])*cos(state[0])*cos(state[1]) - sin(dt*state[8])*sin(state[0])*cos(dt*state[7])*cos(state[1]) + sin(state[1])*cos(dt*state[7])*cos(dt*state[8]), 2));
   out_6444369143388310098[27] = 0;
   out_6444369143388310098[28] = 0;
   out_6444369143388310098[29] = 0;
   out_6444369143388310098[30] = 0;
   out_6444369143388310098[31] = 0;
   out_6444369143388310098[32] = 0;
   out_6444369143388310098[33] = 0;
   out_6444369143388310098[34] = 0;
   out_6444369143388310098[35] = 0;
   out_6444369143388310098[36] = ((sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[7]))*((-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) - (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) - sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2)) + ((-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[7]))*(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2));
   out_6444369143388310098[37] = (-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]))*(-sin(dt*state[7])*sin(state[2])*cos(state[0])*cos(state[1]) + sin(dt*state[8])*sin(state[0])*sin(state[2])*cos(dt*state[7])*cos(state[1]) - sin(state[1])*sin(state[2])*cos(dt*state[7])*cos(dt*state[8]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2)) + ((-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) - (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) - sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]))*(-sin(dt*state[7])*cos(state[0])*cos(state[1])*cos(state[2]) + sin(dt*state[8])*sin(state[0])*cos(dt*state[7])*cos(state[1])*cos(state[2]) - sin(state[1])*cos(dt*state[7])*cos(dt*state[8])*cos(state[2]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2));
   out_6444369143388310098[38] = ((-sin(state[0])*sin(state[2]) - sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]))*(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2)) + ((-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (-sin(state[0])*sin(state[1])*sin(state[2]) - cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) - sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]))*((-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) - (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) - sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2));
   out_6444369143388310098[39] = 0;
   out_6444369143388310098[40] = 0;
   out_6444369143388310098[41] = 0;
   out_6444369143388310098[42] = 0;
   out_6444369143388310098[43] = (-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]))*(dt*(sin(state[0])*cos(state[2]) - sin(state[1])*sin(state[2])*cos(state[0]))*cos(dt*state[7]) - dt*(sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[7])*sin(dt*state[8]) - dt*sin(dt*state[7])*sin(state[2])*cos(dt*state[8])*cos(state[1]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2)) + ((-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) - (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) - sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]))*(dt*(-sin(state[0])*sin(state[2]) - sin(state[1])*cos(state[0])*cos(state[2]))*cos(dt*state[7]) - dt*(sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[7])*sin(dt*state[8]) - dt*sin(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2));
   out_6444369143388310098[44] = (dt*(sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*cos(dt*state[7])*cos(dt*state[8]) - dt*sin(dt*state[8])*sin(state[2])*cos(dt*state[7])*cos(state[1]))*(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2)) + (dt*(sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*cos(dt*state[7])*cos(dt*state[8]) - dt*sin(dt*state[8])*cos(dt*state[7])*cos(state[1])*cos(state[2]))*((-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) - (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) - sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]))/(pow(-(sin(state[0])*sin(state[2]) + sin(state[1])*cos(state[0])*cos(state[2]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*cos(state[2]) - sin(state[2])*cos(state[0]))*sin(dt*state[8])*cos(dt*state[7]) + cos(dt*state[7])*cos(dt*state[8])*cos(state[1])*cos(state[2]), 2) + pow(-(-sin(state[0])*cos(state[2]) + sin(state[1])*sin(state[2])*cos(state[0]))*sin(dt*state[7]) + (sin(state[0])*sin(state[1])*sin(state[2]) + cos(state[0])*cos(state[2]))*sin(dt*state[8])*cos(dt*state[7]) + sin(state[2])*cos(dt*state[7])*cos(dt*state[8])*cos(state[1]), 2));
   out_6444369143388310098[45] = 0;
   out_6444369143388310098[46] = 0;
   out_6444369143388310098[47] = 0;
   out_6444369143388310098[48] = 0;
   out_6444369143388310098[49] = 0;
   out_6444369143388310098[50] = 0;
   out_6444369143388310098[51] = 0;
   out_6444369143388310098[52] = 0;
   out_6444369143388310098[53] = 0;
   out_6444369143388310098[54] = 0;
   out_6444369143388310098[55] = 0;
   out_6444369143388310098[56] = 0;
   out_6444369143388310098[57] = 1;
   out_6444369143388310098[58] = 0;
   out_6444369143388310098[59] = 0;
   out_6444369143388310098[60] = 0;
   out_6444369143388310098[61] = 0;
   out_6444369143388310098[62] = 0;
   out_6444369143388310098[63] = 0;
   out_6444369143388310098[64] = 0;
   out_6444369143388310098[65] = 0;
   out_6444369143388310098[66] = dt;
   out_6444369143388310098[67] = 0;
   out_6444369143388310098[68] = 0;
   out_6444369143388310098[69] = 0;
   out_6444369143388310098[70] = 0;
   out_6444369143388310098[71] = 0;
   out_6444369143388310098[72] = 0;
   out_6444369143388310098[73] = 0;
   out_6444369143388310098[74] = 0;
   out_6444369143388310098[75] = 0;
   out_6444369143388310098[76] = 1;
   out_6444369143388310098[77] = 0;
   out_6444369143388310098[78] = 0;
   out_6444369143388310098[79] = 0;
   out_6444369143388310098[80] = 0;
   out_6444369143388310098[81] = 0;
   out_6444369143388310098[82] = 0;
   out_6444369143388310098[83] = 0;
   out_6444369143388310098[84] = 0;
   out_6444369143388310098[85] = dt;
   out_6444369143388310098[86] = 0;
   out_6444369143388310098[87] = 0;
   out_6444369143388310098[88] = 0;
   out_6444369143388310098[89] = 0;
   out_6444369143388310098[90] = 0;
   out_6444369143388310098[91] = 0;
   out_6444369143388310098[92] = 0;
   out_6444369143388310098[93] = 0;
   out_6444369143388310098[94] = 0;
   out_6444369143388310098[95] = 1;
   out_6444369143388310098[96] = 0;
   out_6444369143388310098[97] = 0;
   out_6444369143388310098[98] = 0;
   out_6444369143388310098[99] = 0;
   out_6444369143388310098[100] = 0;
   out_6444369143388310098[101] = 0;
   out_6444369143388310098[102] = 0;
   out_6444369143388310098[103] = 0;
   out_6444369143388310098[104] = dt;
   out_6444369143388310098[105] = 0;
   out_6444369143388310098[106] = 0;
   out_6444369143388310098[107] = 0;
   out_6444369143388310098[108] = 0;
   out_6444369143388310098[109] = 0;
   out_6444369143388310098[110] = 0;
   out_6444369143388310098[111] = 0;
   out_6444369143388310098[112] = 0;
   out_6444369143388310098[113] = 0;
   out_6444369143388310098[114] = 1;
   out_6444369143388310098[115] = 0;
   out_6444369143388310098[116] = 0;
   out_6444369143388310098[117] = 0;
   out_6444369143388310098[118] = 0;
   out_6444369143388310098[119] = 0;
   out_6444369143388310098[120] = 0;
   out_6444369143388310098[121] = 0;
   out_6444369143388310098[122] = 0;
   out_6444369143388310098[123] = 0;
   out_6444369143388310098[124] = 0;
   out_6444369143388310098[125] = 0;
   out_6444369143388310098[126] = 0;
   out_6444369143388310098[127] = 0;
   out_6444369143388310098[128] = 0;
   out_6444369143388310098[129] = 0;
   out_6444369143388310098[130] = 0;
   out_6444369143388310098[131] = 0;
   out_6444369143388310098[132] = 0;
   out_6444369143388310098[133] = 1;
   out_6444369143388310098[134] = 0;
   out_6444369143388310098[135] = 0;
   out_6444369143388310098[136] = 0;
   out_6444369143388310098[137] = 0;
   out_6444369143388310098[138] = 0;
   out_6444369143388310098[139] = 0;
   out_6444369143388310098[140] = 0;
   out_6444369143388310098[141] = 0;
   out_6444369143388310098[142] = 0;
   out_6444369143388310098[143] = 0;
   out_6444369143388310098[144] = 0;
   out_6444369143388310098[145] = 0;
   out_6444369143388310098[146] = 0;
   out_6444369143388310098[147] = 0;
   out_6444369143388310098[148] = 0;
   out_6444369143388310098[149] = 0;
   out_6444369143388310098[150] = 0;
   out_6444369143388310098[151] = 0;
   out_6444369143388310098[152] = 1;
   out_6444369143388310098[153] = 0;
   out_6444369143388310098[154] = 0;
   out_6444369143388310098[155] = 0;
   out_6444369143388310098[156] = 0;
   out_6444369143388310098[157] = 0;
   out_6444369143388310098[158] = 0;
   out_6444369143388310098[159] = 0;
   out_6444369143388310098[160] = 0;
   out_6444369143388310098[161] = 0;
   out_6444369143388310098[162] = 0;
   out_6444369143388310098[163] = 0;
   out_6444369143388310098[164] = 0;
   out_6444369143388310098[165] = 0;
   out_6444369143388310098[166] = 0;
   out_6444369143388310098[167] = 0;
   out_6444369143388310098[168] = 0;
   out_6444369143388310098[169] = 0;
   out_6444369143388310098[170] = 0;
   out_6444369143388310098[171] = 1;
   out_6444369143388310098[172] = 0;
   out_6444369143388310098[173] = 0;
   out_6444369143388310098[174] = 0;
   out_6444369143388310098[175] = 0;
   out_6444369143388310098[176] = 0;
   out_6444369143388310098[177] = 0;
   out_6444369143388310098[178] = 0;
   out_6444369143388310098[179] = 0;
   out_6444369143388310098[180] = 0;
   out_6444369143388310098[181] = 0;
   out_6444369143388310098[182] = 0;
   out_6444369143388310098[183] = 0;
   out_6444369143388310098[184] = 0;
   out_6444369143388310098[185] = 0;
   out_6444369143388310098[186] = 0;
   out_6444369143388310098[187] = 0;
   out_6444369143388310098[188] = 0;
   out_6444369143388310098[189] = 0;
   out_6444369143388310098[190] = 1;
   out_6444369143388310098[191] = 0;
   out_6444369143388310098[192] = 0;
   out_6444369143388310098[193] = 0;
   out_6444369143388310098[194] = 0;
   out_6444369143388310098[195] = 0;
   out_6444369143388310098[196] = 0;
   out_6444369143388310098[197] = 0;
   out_6444369143388310098[198] = 0;
   out_6444369143388310098[199] = 0;
   out_6444369143388310098[200] = 0;
   out_6444369143388310098[201] = 0;
   out_6444369143388310098[202] = 0;
   out_6444369143388310098[203] = 0;
   out_6444369143388310098[204] = 0;
   out_6444369143388310098[205] = 0;
   out_6444369143388310098[206] = 0;
   out_6444369143388310098[207] = 0;
   out_6444369143388310098[208] = 0;
   out_6444369143388310098[209] = 1;
   out_6444369143388310098[210] = 0;
   out_6444369143388310098[211] = 0;
   out_6444369143388310098[212] = 0;
   out_6444369143388310098[213] = 0;
   out_6444369143388310098[214] = 0;
   out_6444369143388310098[215] = 0;
   out_6444369143388310098[216] = 0;
   out_6444369143388310098[217] = 0;
   out_6444369143388310098[218] = 0;
   out_6444369143388310098[219] = 0;
   out_6444369143388310098[220] = 0;
   out_6444369143388310098[221] = 0;
   out_6444369143388310098[222] = 0;
   out_6444369143388310098[223] = 0;
   out_6444369143388310098[224] = 0;
   out_6444369143388310098[225] = 0;
   out_6444369143388310098[226] = 0;
   out_6444369143388310098[227] = 0;
   out_6444369143388310098[228] = 1;
   out_6444369143388310098[229] = 0;
   out_6444369143388310098[230] = 0;
   out_6444369143388310098[231] = 0;
   out_6444369143388310098[232] = 0;
   out_6444369143388310098[233] = 0;
   out_6444369143388310098[234] = 0;
   out_6444369143388310098[235] = 0;
   out_6444369143388310098[236] = 0;
   out_6444369143388310098[237] = 0;
   out_6444369143388310098[238] = 0;
   out_6444369143388310098[239] = 0;
   out_6444369143388310098[240] = 0;
   out_6444369143388310098[241] = 0;
   out_6444369143388310098[242] = 0;
   out_6444369143388310098[243] = 0;
   out_6444369143388310098[244] = 0;
   out_6444369143388310098[245] = 0;
   out_6444369143388310098[246] = 0;
   out_6444369143388310098[247] = 1;
   out_6444369143388310098[248] = 0;
   out_6444369143388310098[249] = 0;
   out_6444369143388310098[250] = 0;
   out_6444369143388310098[251] = 0;
   out_6444369143388310098[252] = 0;
   out_6444369143388310098[253] = 0;
   out_6444369143388310098[254] = 0;
   out_6444369143388310098[255] = 0;
   out_6444369143388310098[256] = 0;
   out_6444369143388310098[257] = 0;
   out_6444369143388310098[258] = 0;
   out_6444369143388310098[259] = 0;
   out_6444369143388310098[260] = 0;
   out_6444369143388310098[261] = 0;
   out_6444369143388310098[262] = 0;
   out_6444369143388310098[263] = 0;
   out_6444369143388310098[264] = 0;
   out_6444369143388310098[265] = 0;
   out_6444369143388310098[266] = 1;
   out_6444369143388310098[267] = 0;
   out_6444369143388310098[268] = 0;
   out_6444369143388310098[269] = 0;
   out_6444369143388310098[270] = 0;
   out_6444369143388310098[271] = 0;
   out_6444369143388310098[272] = 0;
   out_6444369143388310098[273] = 0;
   out_6444369143388310098[274] = 0;
   out_6444369143388310098[275] = 0;
   out_6444369143388310098[276] = 0;
   out_6444369143388310098[277] = 0;
   out_6444369143388310098[278] = 0;
   out_6444369143388310098[279] = 0;
   out_6444369143388310098[280] = 0;
   out_6444369143388310098[281] = 0;
   out_6444369143388310098[282] = 0;
   out_6444369143388310098[283] = 0;
   out_6444369143388310098[284] = 0;
   out_6444369143388310098[285] = 1;
   out_6444369143388310098[286] = 0;
   out_6444369143388310098[287] = 0;
   out_6444369143388310098[288] = 0;
   out_6444369143388310098[289] = 0;
   out_6444369143388310098[290] = 0;
   out_6444369143388310098[291] = 0;
   out_6444369143388310098[292] = 0;
   out_6444369143388310098[293] = 0;
   out_6444369143388310098[294] = 0;
   out_6444369143388310098[295] = 0;
   out_6444369143388310098[296] = 0;
   out_6444369143388310098[297] = 0;
   out_6444369143388310098[298] = 0;
   out_6444369143388310098[299] = 0;
   out_6444369143388310098[300] = 0;
   out_6444369143388310098[301] = 0;
   out_6444369143388310098[302] = 0;
   out_6444369143388310098[303] = 0;
   out_6444369143388310098[304] = 1;
   out_6444369143388310098[305] = 0;
   out_6444369143388310098[306] = 0;
   out_6444369143388310098[307] = 0;
   out_6444369143388310098[308] = 0;
   out_6444369143388310098[309] = 0;
   out_6444369143388310098[310] = 0;
   out_6444369143388310098[311] = 0;
   out_6444369143388310098[312] = 0;
   out_6444369143388310098[313] = 0;
   out_6444369143388310098[314] = 0;
   out_6444369143388310098[315] = 0;
   out_6444369143388310098[316] = 0;
   out_6444369143388310098[317] = 0;
   out_6444369143388310098[318] = 0;
   out_6444369143388310098[319] = 0;
   out_6444369143388310098[320] = 0;
   out_6444369143388310098[321] = 0;
   out_6444369143388310098[322] = 0;
   out_6444369143388310098[323] = 1;
}
void h_4(double *state, double *unused, double *out_7403012963842053373) {
   out_7403012963842053373[0] = state[6] + state[9];
   out_7403012963842053373[1] = state[7] + state[10];
   out_7403012963842053373[2] = state[8] + state[11];
}
void H_4(double *state, double *unused, double *out_1088510956736073746) {
   out_1088510956736073746[0] = 0;
   out_1088510956736073746[1] = 0;
   out_1088510956736073746[2] = 0;
   out_1088510956736073746[3] = 0;
   out_1088510956736073746[4] = 0;
   out_1088510956736073746[5] = 0;
   out_1088510956736073746[6] = 1;
   out_1088510956736073746[7] = 0;
   out_1088510956736073746[8] = 0;
   out_1088510956736073746[9] = 1;
   out_1088510956736073746[10] = 0;
   out_1088510956736073746[11] = 0;
   out_1088510956736073746[12] = 0;
   out_1088510956736073746[13] = 0;
   out_1088510956736073746[14] = 0;
   out_1088510956736073746[15] = 0;
   out_1088510956736073746[16] = 0;
   out_1088510956736073746[17] = 0;
   out_1088510956736073746[18] = 0;
   out_1088510956736073746[19] = 0;
   out_1088510956736073746[20] = 0;
   out_1088510956736073746[21] = 0;
   out_1088510956736073746[22] = 0;
   out_1088510956736073746[23] = 0;
   out_1088510956736073746[24] = 0;
   out_1088510956736073746[25] = 1;
   out_1088510956736073746[26] = 0;
   out_1088510956736073746[27] = 0;
   out_1088510956736073746[28] = 1;
   out_1088510956736073746[29] = 0;
   out_1088510956736073746[30] = 0;
   out_1088510956736073746[31] = 0;
   out_1088510956736073746[32] = 0;
   out_1088510956736073746[33] = 0;
   out_1088510956736073746[34] = 0;
   out_1088510956736073746[35] = 0;
   out_1088510956736073746[36] = 0;
   out_1088510956736073746[37] = 0;
   out_1088510956736073746[38] = 0;
   out_1088510956736073746[39] = 0;
   out_1088510956736073746[40] = 0;
   out_1088510956736073746[41] = 0;
   out_1088510956736073746[42] = 0;
   out_1088510956736073746[43] = 0;
   out_1088510956736073746[44] = 1;
   out_1088510956736073746[45] = 0;
   out_1088510956736073746[46] = 0;
   out_1088510956736073746[47] = 1;
   out_1088510956736073746[48] = 0;
   out_1088510956736073746[49] = 0;
   out_1088510956736073746[50] = 0;
   out_1088510956736073746[51] = 0;
   out_1088510956736073746[52] = 0;
   out_1088510956736073746[53] = 0;
}
void h_10(double *state, double *unused, double *out_6693233631074133649) {
   out_6693233631074133649[0] = 9.8100000000000005*sin(state[1]) - state[4]*state[8] + state[5]*state[7] + state[12] + state[15];
   out_6693233631074133649[1] = -9.8100000000000005*sin(state[0])*cos(state[1]) + state[3]*state[8] - state[5]*state[6] + state[13] + state[16];
   out_6693233631074133649[2] = -9.8100000000000005*cos(state[0])*cos(state[1]) - state[3]*state[7] + state[4]*state[6] + state[14] + state[17];
}
void H_10(double *state, double *unused, double *out_4342710343640918055) {
   out_4342710343640918055[0] = 0;
   out_4342710343640918055[1] = 9.8100000000000005*cos(state[1]);
   out_4342710343640918055[2] = 0;
   out_4342710343640918055[3] = 0;
   out_4342710343640918055[4] = -state[8];
   out_4342710343640918055[5] = state[7];
   out_4342710343640918055[6] = 0;
   out_4342710343640918055[7] = state[5];
   out_4342710343640918055[8] = -state[4];
   out_4342710343640918055[9] = 0;
   out_4342710343640918055[10] = 0;
   out_4342710343640918055[11] = 0;
   out_4342710343640918055[12] = 1;
   out_4342710343640918055[13] = 0;
   out_4342710343640918055[14] = 0;
   out_4342710343640918055[15] = 1;
   out_4342710343640918055[16] = 0;
   out_4342710343640918055[17] = 0;
   out_4342710343640918055[18] = -9.8100000000000005*cos(state[0])*cos(state[1]);
   out_4342710343640918055[19] = 9.8100000000000005*sin(state[0])*sin(state[1]);
   out_4342710343640918055[20] = 0;
   out_4342710343640918055[21] = state[8];
   out_4342710343640918055[22] = 0;
   out_4342710343640918055[23] = -state[6];
   out_4342710343640918055[24] = -state[5];
   out_4342710343640918055[25] = 0;
   out_4342710343640918055[26] = state[3];
   out_4342710343640918055[27] = 0;
   out_4342710343640918055[28] = 0;
   out_4342710343640918055[29] = 0;
   out_4342710343640918055[30] = 0;
   out_4342710343640918055[31] = 1;
   out_4342710343640918055[32] = 0;
   out_4342710343640918055[33] = 0;
   out_4342710343640918055[34] = 1;
   out_4342710343640918055[35] = 0;
   out_4342710343640918055[36] = 9.8100000000000005*sin(state[0])*cos(state[1]);
   out_4342710343640918055[37] = 9.8100000000000005*sin(state[1])*cos(state[0]);
   out_4342710343640918055[38] = 0;
   out_4342710343640918055[39] = -state[7];
   out_4342710343640918055[40] = state[6];
   out_4342710343640918055[41] = 0;
   out_4342710343640918055[42] = state[4];
   out_4342710343640918055[43] = -state[3];
   out_4342710343640918055[44] = 0;
   out_4342710343640918055[45] = 0;
   out_4342710343640918055[46] = 0;
   out_4342710343640918055[47] = 0;
   out_4342710343640918055[48] = 0;
   out_4342710343640918055[49] = 0;
   out_4342710343640918055[50] = 1;
   out_4342710343640918055[51] = 0;
   out_4342710343640918055[52] = 0;
   out_4342710343640918055[53] = 1;
}
void h_13(double *state, double *unused, double *out_5560317675039026719) {
   out_5560317675039026719[0] = state[3];
   out_5560317675039026719[1] = state[4];
   out_5560317675039026719[2] = state[5];
}
void H_13(double *state, double *unused, double *out_2123762868596259055) {
   out_2123762868596259055[0] = 0;
   out_2123762868596259055[1] = 0;
   out_2123762868596259055[2] = 0;
   out_2123762868596259055[3] = 1;
   out_2123762868596259055[4] = 0;
   out_2123762868596259055[5] = 0;
   out_2123762868596259055[6] = 0;
   out_2123762868596259055[7] = 0;
   out_2123762868596259055[8] = 0;
   out_2123762868596259055[9] = 0;
   out_2123762868596259055[10] = 0;
   out_2123762868596259055[11] = 0;
   out_2123762868596259055[12] = 0;
   out_2123762868596259055[13] = 0;
   out_2123762868596259055[14] = 0;
   out_2123762868596259055[15] = 0;
   out_2123762868596259055[16] = 0;
   out_2123762868596259055[17] = 0;
   out_2123762868596259055[18] = 0;
   out_2123762868596259055[19] = 0;
   out_2123762868596259055[20] = 0;
   out_2123762868596259055[21] = 0;
   out_2123762868596259055[22] = 1;
   out_2123762868596259055[23] = 0;
   out_2123762868596259055[24] = 0;
   out_2123762868596259055[25] = 0;
   out_2123762868596259055[26] = 0;
   out_2123762868596259055[27] = 0;
   out_2123762868596259055[28] = 0;
   out_2123762868596259055[29] = 0;
   out_2123762868596259055[30] = 0;
   out_2123762868596259055[31] = 0;
   out_2123762868596259055[32] = 0;
   out_2123762868596259055[33] = 0;
   out_2123762868596259055[34] = 0;
   out_2123762868596259055[35] = 0;
   out_2123762868596259055[36] = 0;
   out_2123762868596259055[37] = 0;
   out_2123762868596259055[38] = 0;
   out_2123762868596259055[39] = 0;
   out_2123762868596259055[40] = 0;
   out_2123762868596259055[41] = 1;
   out_2123762868596259055[42] = 0;
   out_2123762868596259055[43] = 0;
   out_2123762868596259055[44] = 0;
   out_2123762868596259055[45] = 0;
   out_2123762868596259055[46] = 0;
   out_2123762868596259055[47] = 0;
   out_2123762868596259055[48] = 0;
   out_2123762868596259055[49] = 0;
   out_2123762868596259055[50] = 0;
   out_2123762868596259055[51] = 0;
   out_2123762868596259055[52] = 0;
   out_2123762868596259055[53] = 0;
}
void h_14(double *state, double *unused, double *out_4024393874269291964) {
   out_4024393874269291964[0] = state[6];
   out_4024393874269291964[1] = state[7];
   out_4024393874269291964[2] = state[8];
}
void H_14(double *state, double *unused, double *out_2874729899603410783) {
   out_2874729899603410783[0] = 0;
   out_2874729899603410783[1] = 0;
   out_2874729899603410783[2] = 0;
   out_2874729899603410783[3] = 0;
   out_2874729899603410783[4] = 0;
   out_2874729899603410783[5] = 0;
   out_2874729899603410783[6] = 1;
   out_2874729899603410783[7] = 0;
   out_2874729899603410783[8] = 0;
   out_2874729899603410783[9] = 0;
   out_2874729899603410783[10] = 0;
   out_2874729899603410783[11] = 0;
   out_2874729899603410783[12] = 0;
   out_2874729899603410783[13] = 0;
   out_2874729899603410783[14] = 0;
   out_2874729899603410783[15] = 0;
   out_2874729899603410783[16] = 0;
   out_2874729899603410783[17] = 0;
   out_2874729899603410783[18] = 0;
   out_2874729899603410783[19] = 0;
   out_2874729899603410783[20] = 0;
   out_2874729899603410783[21] = 0;
   out_2874729899603410783[22] = 0;
   out_2874729899603410783[23] = 0;
   out_2874729899603410783[24] = 0;
   out_2874729899603410783[25] = 1;
   out_2874729899603410783[26] = 0;
   out_2874729899603410783[27] = 0;
   out_2874729899603410783[28] = 0;
   out_2874729899603410783[29] = 0;
   out_2874729899603410783[30] = 0;
   out_2874729899603410783[31] = 0;
   out_2874729899603410783[32] = 0;
   out_2874729899603410783[33] = 0;
   out_2874729899603410783[34] = 0;
   out_2874729899603410783[35] = 0;
   out_2874729899603410783[36] = 0;
   out_2874729899603410783[37] = 0;
   out_2874729899603410783[38] = 0;
   out_2874729899603410783[39] = 0;
   out_2874729899603410783[40] = 0;
   out_2874729899603410783[41] = 0;
   out_2874729899603410783[42] = 0;
   out_2874729899603410783[43] = 0;
   out_2874729899603410783[44] = 1;
   out_2874729899603410783[45] = 0;
   out_2874729899603410783[46] = 0;
   out_2874729899603410783[47] = 0;
   out_2874729899603410783[48] = 0;
   out_2874729899603410783[49] = 0;
   out_2874729899603410783[50] = 0;
   out_2874729899603410783[51] = 0;
   out_2874729899603410783[52] = 0;
   out_2874729899603410783[53] = 0;
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

void pose_update_4(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<3, 3, 0>(in_x, in_P, h_4, H_4, NULL, in_z, in_R, in_ea, MAHA_THRESH_4);
}
void pose_update_10(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<3, 3, 0>(in_x, in_P, h_10, H_10, NULL, in_z, in_R, in_ea, MAHA_THRESH_10);
}
void pose_update_13(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<3, 3, 0>(in_x, in_P, h_13, H_13, NULL, in_z, in_R, in_ea, MAHA_THRESH_13);
}
void pose_update_14(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<3, 3, 0>(in_x, in_P, h_14, H_14, NULL, in_z, in_R, in_ea, MAHA_THRESH_14);
}
void pose_err_fun(double *nom_x, double *delta_x, double *out_2294198930716030722) {
  err_fun(nom_x, delta_x, out_2294198930716030722);
}
void pose_inv_err_fun(double *nom_x, double *true_x, double *out_4974602922426709952) {
  inv_err_fun(nom_x, true_x, out_4974602922426709952);
}
void pose_H_mod_fun(double *state, double *out_4759570850015884975) {
  H_mod_fun(state, out_4759570850015884975);
}
void pose_f_fun(double *state, double dt, double *out_1340898844102295762) {
  f_fun(state,  dt, out_1340898844102295762);
}
void pose_F_fun(double *state, double dt, double *out_6444369143388310098) {
  F_fun(state,  dt, out_6444369143388310098);
}
void pose_h_4(double *state, double *unused, double *out_7403012963842053373) {
  h_4(state, unused, out_7403012963842053373);
}
void pose_H_4(double *state, double *unused, double *out_1088510956736073746) {
  H_4(state, unused, out_1088510956736073746);
}
void pose_h_10(double *state, double *unused, double *out_6693233631074133649) {
  h_10(state, unused, out_6693233631074133649);
}
void pose_H_10(double *state, double *unused, double *out_4342710343640918055) {
  H_10(state, unused, out_4342710343640918055);
}
void pose_h_13(double *state, double *unused, double *out_5560317675039026719) {
  h_13(state, unused, out_5560317675039026719);
}
void pose_H_13(double *state, double *unused, double *out_2123762868596259055) {
  H_13(state, unused, out_2123762868596259055);
}
void pose_h_14(double *state, double *unused, double *out_4024393874269291964) {
  h_14(state, unused, out_4024393874269291964);
}
void pose_H_14(double *state, double *unused, double *out_2874729899603410783) {
  H_14(state, unused, out_2874729899603410783);
}
void pose_predict(double *in_x, double *in_P, double *in_Q, double dt) {
  predict(in_x, in_P, in_Q, dt);
}
}

const EKF pose = {
  .name = "pose",
  .kinds = { 4, 10, 13, 14 },
  .feature_kinds = {  },
  .f_fun = pose_f_fun,
  .F_fun = pose_F_fun,
  .err_fun = pose_err_fun,
  .inv_err_fun = pose_inv_err_fun,
  .H_mod_fun = pose_H_mod_fun,
  .predict = pose_predict,
  .hs = {
    { 4, pose_h_4 },
    { 10, pose_h_10 },
    { 13, pose_h_13 },
    { 14, pose_h_14 },
  },
  .Hs = {
    { 4, pose_H_4 },
    { 10, pose_H_10 },
    { 13, pose_H_13 },
    { 14, pose_H_14 },
  },
  .updates = {
    { 4, pose_update_4 },
    { 10, pose_update_10 },
    { 13, pose_update_13 },
    { 14, pose_update_14 },
  },
  .Hes = {
  },
  .sets = {
  },
  .extra_routines = {
  },
};

ekf_lib_init(pose)
