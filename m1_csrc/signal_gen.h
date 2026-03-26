/* See COPYING.txt for license details. */

/*
 * signal_gen.h
 *
 * Configurable square-wave signal generator using the M1 buzzer timer.
 * Outputs a continuous square wave on the speaker (SPK_CTRL) pin at a
 * user-selectable frequency.
 *
 * Inspired by the signal_generator FAP from:
 *   https://github.com/flipperdevices/flipperzero-good-faps
 *
 * M1 Project
 */

#ifndef SIGNAL_GEN_H_
#define SIGNAL_GEN_H_

/* Entry point — shows frequency selector, outputs signal until BACK */
void signal_gen_run(void);

#endif /* SIGNAL_GEN_H_ */
