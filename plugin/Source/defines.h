#ifndef DEFINES_H
#define DEFINES_H

//everyone's favorite header file... a list of magic numbers!
//At least they're all in one place and not scattered throughout the codebase like rice at a wedding.

#define NUM_SYNTH_VOICES 16

//resonators
#define NUM_WAVEGUIDE_RESONATORS 8
#define NUM_WAVEGUIDE_RESONATOR_BANKS 2
#define NUM_MODAL_RESONATORS 8
#define NUM_MODAL_RESONATOR_BANKS 2
#define MAX_FILTER_FREQUENCY 22000.0f;

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

//misc. UI stuff
#define WINDOW_WIDTH 1300
#define WINDOW_HEIGHT 900
#define BOX_HEADER_HEIGHT 28
#define TOP_MENU_BAR_HEIGHT 40
#define PARAM_BOX_XSMALL_HEIGHT 105
#define PARAM_BOX_SMALL_HEIGHT 170
#define PARAM_BOX_MEDIUM_HEIGHT 235
#define KNOB_W_XSMALL 25
#define KNOB_H_XSMALL 32
#define KNOB_W_SMALL 42
#define KNOB_H_SMALL 57
#define KNOB_W 52
#define KNOB_H 65
#define EXCITER_BOX_WIDTH 230
#define RESONATOR_BANK_BOX_HEIGHT 405

#endif //DEFINES_H
