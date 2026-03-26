/* See COPYING.txt for license details. */

/*
 * music_player.h
 *
 * Flipper Music Format (.fmf) player for M1.
 * Parses BPM/Duration/Octave/Notes from an SD card file and plays
 * the note sequence through the M1 buzzer.
 *
 * File format (Flipper Music Format v0):
 *   Filetype: Flipper Music Format
 *   Version: 0
 *   BPM: <beats-per-minute>
 *   Duration: <default note duration: 1=whole 2=half 4=quarter 8=eighth 16 32>
 *   Octave: <default octave 0-8>
 *   Notes: <comma-separated note tokens>
 *
 * Note token format: [duration]NOTE[#][octave][.]
 *   duration : optional leading integer (overrides default Duration)
 *   NOTE     : A-G (uppercase or lowercase) or P for pause
 *   #        : optional sharp
 *   octave   : optional octave digit (overrides default Octave)
 *   .        : optional dot(s) — each multiplies remaining duration by 1.5
 *
 * M1 Project
 */

#ifndef MUSIC_PLAYER_H_
#define MUSIC_PLAYER_H_

/* SD card directory for music files */
#define MUSIC_PLAYER_DIR    "0:/Music"

/* Entry point — browse SD:/Music, select a .fmf file, play it */
void music_player_run(void);

#endif /* MUSIC_PLAYER_H_ */
