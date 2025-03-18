#ifndef DEFINES_H
#define DEFINES_H

//everyone's favorite header file... a list of magic numbers!
//At least they're all in one place and not scattered throughout the codebase like rice at a wedding.

// Define M_PI for Windows builds where it's not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NUM_SYNTH_VOICES 16

//resonators
#define NUM_RESONATORS 8
#define NUM_RESONATOR_BANKS 4

//parameter values
#define MIN_FILTER_FREQUENCY 10.0f
#define MAX_FILTER_FREQUENCY 20000.0f
#define FREQUENCY_KNOB_SKEW 0.3f

//exciters
#define NUM_IMPULSE_EXCITERS 1
#define NUM_NOISE_EXCITERS 1
#define NUM_IMPULSE_TRAIN_EXCITERS 1
#define NUM_SAMPLE_EXCITERS 1

//modulation sources
#define NUM_LFOS 4
#define NUM_ENVELOPES 4
#define NUM_RANDOMS 4
#define NUM_MSEGS 4
#define NUM_MACROS 6

//effects
#define MAX_DELAY_IN_SECONDS 2.0f

//misc. UI stuff
#define WINDOW_WIDTH (1300 - 18)
#define WINDOW_HEIGHT (900 - 11)
#define BOX_HEADER_HEIGHT 28
#define TOP_MENU_BAR_HEIGHT 40
#define TITLE_BAR_HEIGHT 28
#define PARAM_BOX_XSMALL_HEIGHT 105
#define PARAM_BOX_SMALL_HEIGHT 170
#define PARAM_BOX_MEDIUM_HEIGHT 235
#define PARAM_BOX_LARGE_HEIGHT 300
#define KNOB_W_XSMALL 25
#define KNOB_H_XSMALL 32
#define KNOB_W_SMALL 42
#define KNOB_H_SMALL 57
#define KNOB_W 52
#define KNOB_H 65
#define EXCITER_BOX_WIDTH (4 * KNOB_W + 4 + 4)
#define MODULATION_BOX_WIDTH (8 * KNOB_W + 4 + 4)
#define RESONATOR_BANK_BOX_WIDTH (2 * MODULATION_BOX_WIDTH)
#define RESONATOR_BANK_BOX_HEIGHT (405 + 65 + 65)

#endif //DEFINES_H
