#include <math.h>



union uData {

    bool b; char c; unsigned char uc; short s; unsigned short us;

    int i; unsigned int ui; float f; double d;

    long long int i64; unsigned long long int ui64;

    char *str; unsigned char *bytes;

};



int __stdcall DllMain(void *module, unsigned int reason, void *reserved) {

    (void)module;

    (void)reason;

    (void)reserved;

    return 1;

}



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

    double t_last_switch;

    int    active_leg;

    bool   stop_charge_pending;

};



extern "C" __declspec(dllexport) void jet_stream(void **opaque, double t, union uData *data) {

    sINSTANCE *inst = (sINSTANCE *)*opaque;

    if (!inst) {

        inst = new sINSTANCE();

        inst->state               = STATE_CHARGE;

        inst->t_state_start       = 0.0;

        inst->t_cycle_start       = 0.0;

        inst->t_last_switch       = 0.0;

        inst->active_leg          = 1;

        inst->stop_charge_pending = false;



        // Target 780V on HV bus

        inst->target_hv           = 800.0;

        *opaque = (void *)inst;

    }



    // Pin mapping

    double  V_HV    = data[0].d; // Storage capacitor C_BUF

    double  V_DR2   = data[1].d; // Drain M2 feedback

    double  V_DR1   = data[2].d; // Drain M1 feedback

    double &b_g1    = data[3].d; // Gate M1 (0.0 / 1.0 V)

    double &c_g2    = data[4].d; // Gate M2 (0.0 / 1.0 V)

    double &OUT_SIC = data[5].d; // Gate SiC (0.0 / 1.0 V)



    // Hardware tank timings matched to actual 1.45 us half-wave (345 kHz)

    const double T_HALF_NATURAL = 1.45e-6;         // 1.45 us half-period

    const double T_MIN_PULSE    = 0.90e-6;         // Blanking gate noise

    const double T_MAX_TIMEOUT  = 1.65e-6;         // Fallback switch threshold

    const double T_OVERLAP      = 50.0e-9;         // 50 ns current-fed overlap

    const double V_ZVS_THRESH   = 2.0;             // Zero-crossing comparator threshold



    // Repetition sequence

    const double T_BLANK_PRE    = 1.0e-6;          // 1 us pre-shot deadtime

    const double T_PULSE_SIC    = 14.0e-9;         // 14 ns SiC stroke

    const double T_BLANK_POST   = 2.0e-6;          // 2 us plasma relaxation

    const double T_CYCLE_REP    = 100.0e-6;        // 10 kHz master cycle (100 us)



    switch (inst->state) {



        case STATE_CHARGE: {

            OUT_SIC = 0.0;



            if (V_HV >= inst->target_hv) {

                inst->stop_charge_pending = true;

            }



            double t_leg = t - inst->t_last_switch;



            // ZVS detection strictly after blanking window

            bool zvs_event = false;

            if (t_leg >= T_MIN_PULSE) {

                if (inst->active_leg == 1 && V_DR2 <= V_ZVS_THRESH) {

                    zvs_event = true;

                } else if (inst->active_leg == 2 && V_DR1 <= V_ZVS_THRESH) {

                    zvs_event = true;

                }

            }



            // Fallback timeout

            if (t_leg >= T_MAX_TIMEOUT) {

                zvs_event = true;

            }



            if (zvs_event) {

                if (inst->stop_charge_pending) {

                    b_g1 = 0.0;

                    c_g2 = 0.0;

                    inst->stop_charge_pending = false;

                    inst->state         = STATE_HOLD;

                    inst->t_state_start = t;

                    break;

                }



                // Switch active leg

                inst->active_leg    = (inst->active_leg == 1) ? 2 : 1;

                inst->t_last_switch = t;

                t_leg               = 0.0;

            }



            // Current-fed gate signals with overlap

            if (inst->active_leg == 1) {

                b_g1 = 1.0;

                c_g2 = (t_leg < T_OVERLAP) ? 1.0 : 0.0;

            } else {

                c_g2 = 1.0;

                b_g1 = (t_leg < T_OVERLAP) ? 1.0 : 0.0;

            }

            break;

        }



        case STATE_HOLD: {

            b_g1    = 0.0;

            c_g2    = 0.0;

            OUT_SIC = 0.0;



            if ((t - inst->t_cycle_start) >= (T_CYCLE_REP - T_BLANK_PRE)) {

                inst->state         = STATE_BLANK_PRE;

                inst->t_state_start = t;

            }

            break;

        }



        case STATE_BLANK_PRE: {

            b_g1    = 0.0;

            c_g2    = 0.0;

            OUT_SIC = 0.0;

            if ((t - inst->t_cycle_start) >= T_CYCLE_REP) {
                inst->state         = STATE_FIRE_SIC;
                inst->t_state_start = t;

                while ((t - inst->t_cycle_start) >= T_CYCLE_REP) {
                    inst->t_cycle_start += T_CYCLE_REP;
                }
            }

            break;
        }


        case STATE_FIRE_SIC: {

            b_g1    = 0.0;

            c_g2    = 0.0;

            OUT_SIC = 1.0;



            if ((t - inst->t_state_start) >= T_PULSE_SIC) {

                OUT_SIC             = 0.0;

                inst->state         = STATE_BLANK_POST;

                inst->t_state_start = t;

            }

            break;

        }



        case STATE_BLANK_POST: {

            b_g1    = 0.0;

            c_g2    = 0.0;

            OUT_SIC = 0.0;



            if ((t - inst->t_state_start) >= T_BLANK_POST) {

                inst->state               = STATE_CHARGE;

                inst->t_state_start       = t;

                inst->t_last_switch       = t;

                inst->active_leg          = 1;

                inst->stop_charge_pending = false;

            }

            break;

        }

    }

}
