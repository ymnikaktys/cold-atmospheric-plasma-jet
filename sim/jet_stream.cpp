#include <math.h>



union uData {

    bool b; char c; unsigned char uc; short s; unsigned short us;

    int i; unsigned int ui; float f; double d;

    long long int i64; unsigned long long int ui64;

    char *str; unsigned char *bytes;

};



int __stdcall DllMain(void *module, unsigned int reason, void *reserved) { return 1; }



enum State {

    STATE_CHARGE = 0,

    STATE_HOLD,

    STATE_BLANK_PRE,

    STATE_FIRE_SIC,

    STATE_BLANK_POST

};



struct sINSTANCE {

    State  state;

    double t_state_start;

    double t_cycle_start;

    double target_hv;

};



extern "C" __declspec(dllexport) void jet_stream(void **opaque, double t, union uData *data) {

    sINSTANCE *inst = (sINSTANCE *)*opaque;

    if (!inst) {

        inst = new sINSTANCE();

        inst->state         = STATE_CHARGE;

        inst->t_state_start = 0.0;

        inst->t_cycle_start = 0.0;

        // Cutoff at 780V: choke energy runout lands perfectly at 835-850V

        inst->target_hv     = 850.0;

        *opaque = inst;

    }



    // Pin mapping

    double  V_HV    = data[0].d; // Storage capacitor

    double  V_DR2   = data[1].d; // Drain M2 (unused in fixed-PWM mode)

    double  V_DR1   = data[2].d; // Drain M1 (unused in fixed-PWM mode)

    double &b_g1    = data[3].d; // Gate M1: STRICTLY 0.0 / 1.0 V

    double &c_g2    = data[4].d; // Gate M2: STRICTLY 0.0 / 1.0 V

    double &OUT_SIC = data[5].d; // SiC Gate: STRICTLY 0.0 / 1.0 V



    // Tuned for L1=2.5uH, L2=2.5uH, C1=220nF (f_res ~ 98 kHz)

    const double F_PP_FREQ    = 98.0e3;          // 98 kHz resonance

    const double T_PERIOD     = 1.0 / F_PP_FREQ; // ~10.2 us

    const double T_OVERLAP    = 120.0e-9;        // 120 ns Current-fed overlap



    const double T_BLANK_PRE  = 1.0e-6;          // 1 us Pre-shot deadtime

    const double T_PULSE_SIC  = 12.0e-9;         // 12 ns SiC stroke

    const double T_BLANK_POST = 2.0e-6;          // 2 us Plasma relaxation

    const double T_CYCLE_REP  = 100.0e-6;        // 10 kHz Master repetition



    switch (inst->state) {



        case STATE_CHARGE:

            OUT_SIC = 0.0;



            // Cutoff check

            if (V_HV >= inst->target_hv) {

                b_g1 = 0.0;

                c_g2 = 0.0;

                inst->state = STATE_HOLD;

                inst->t_state_start = t;

                break;

            }



            // Fixed-frequency symmetric PWM with current-fed overlap

            {

                double t_rel   = t - inst->t_state_start;

                double t_phase = fmod(t_rel, T_PERIOD);

                double t_half  = T_PERIOD * 0.5;



                bool dr1 = (t_phase < (t_half + T_OVERLAP));

                bool dr2 = (t_phase >= t_half) || (t_phase < T_OVERLAP);



                b_g1 = dr1 ? 1.0 : 0.0;

                c_g2 = dr2 ? 1.0 : 0.0;

            }

            break;



        case STATE_HOLD:

            b_g1 = 0.0;

            c_g2 = 0.0;

            OUT_SIC = 0.0;



            if ((t - inst->t_cycle_start) >= (T_CYCLE_REP - T_BLANK_PRE)) {

                inst->state = STATE_BLANK_PRE;

                inst->t_state_start = t;

            }

            break;



        case STATE_BLANK_PRE:

            b_g1 = 0.0;

            c_g2 = 0.0;

            OUT_SIC = 0.0;



            if ((t - inst->t_cycle_start) >= T_CYCLE_REP) {

                inst->state = STATE_FIRE_SIC;

                inst->t_state_start = t;

                inst->t_cycle_start += T_CYCLE_REP;

            }

            break;



        case STATE_FIRE_SIC:

            b_g1 = 0.0;

            c_g2 = 0.0;

            OUT_SIC = 1.0;



            if ((t - inst->t_state_start) >= T_PULSE_SIC) {

                OUT_SIC = 0.0;

                inst->state = STATE_BLANK_POST;

                inst->t_state_start = t;

            }

            break;



        case STATE_BLANK_POST:

            b_g1 = 0.0;

            c_g2 = 0.0;

            OUT_SIC = 0.0;



            if ((t - inst->t_state_start) >= T_BLANK_POST) {

                inst->state         = STATE_CHARGE;

                inst->t_state_start = t;

            }

            break;

    }

}
