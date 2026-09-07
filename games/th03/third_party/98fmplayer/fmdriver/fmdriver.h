#ifndef MYON_FMDRIVER_H_INCLUDED
#define MYON_FMDRIVER_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

/* th03-port extraction patch.
 *
 * Upstream includes ppz8.h here. That pulls stdatomic.h and the visualizer's
 * level-data API into this otherwise platform-neutral sequencer header, and
 * MSVC 14.41 cannot compile that C11 atomic header in this C target. TH03 ships
 * no PPZ8 data. Keep the exact callback layout needed by fmdriver_pmd.c and bind
 * both PPZ8 pointers to NULL in the port shim. All driver dereferences are
 * already guarded by those pointers.
 *
 * This declaration is copied from the same BSD-2-Clause upstream commit named
 * in ../UPSTREAM.md; no behavior or ABI is changed.
 */
struct ppz8;
struct ppz8_functbl {
  void (*channel_play)(struct ppz8 *ppz8, uint8_t channel, uint8_t voice);
  void (*channel_stop)(struct ppz8 *ppz8, uint8_t channel);
  void (*channel_volume)(struct ppz8 *ppz8, uint8_t channel, uint8_t vol);
  void (*channel_freq)(struct ppz8 *ppz8, uint8_t channel, uint32_t freq);
  void (*channel_loopoffset)(struct ppz8 *ppz8, uint8_t channel,
                             uint32_t startoff, uint32_t endoff);
  void (*channel_pan)(struct ppz8 *ppz8, uint8_t channel, uint8_t pan);
  void (*total_volume)(struct ppz8 *ppz8, uint8_t vol);
  void (*channel_loop_voice)(struct ppz8 *ppz8, uint8_t channel, uint8_t voice);
  uint32_t (*voice_length)(struct ppz8 *ppz8, uint8_t voice);
};

enum {
  FMDRIVER_TRACK_FM_1,
  FMDRIVER_TRACK_FM_2,
  FMDRIVER_TRACK_FM_3,
  FMDRIVER_TRACK_FM_3_EX_1,
  FMDRIVER_TRACK_FM_3_EX_2,
  FMDRIVER_TRACK_FM_3_EX_3,
  FMDRIVER_TRACK_FM_4,
  FMDRIVER_TRACK_FM_5,
  FMDRIVER_TRACK_FM_6,
  FMDRIVER_TRACK_SSG_1,
  FMDRIVER_TRACK_SSG_2,
  FMDRIVER_TRACK_SSG_3,
  FMDRIVER_TRACK_ADPCM,
  FMDRIVER_TRACK_PPZ8_1,
  FMDRIVER_TRACK_PPZ8_2,
  FMDRIVER_TRACK_PPZ8_3,
  FMDRIVER_TRACK_PPZ8_4,
  FMDRIVER_TRACK_PPZ8_5,
  FMDRIVER_TRACK_PPZ8_6,
  FMDRIVER_TRACK_PPZ8_7,
  FMDRIVER_TRACK_PPZ8_8,
  FMDRIVER_TRACK_NUM
};
enum {
  // 1 line = 80 characters, may contain half-width doublebyte characters
  FMDRIVER_TITLE_BUFLEN = 80*2+1,

  FMDRIVER_PCMCOUNT = 4,
};

enum fmdriver_track_type {
  FMDRIVER_TRACKTYPE_FM,
  FMDRIVER_TRACKTYPE_SSG,
  FMDRIVER_TRACKTYPE_ADPCM,
  FMDRIVER_TRACKTYPE_PPZ8,
  FMDRIVER_TRACKTYPE_CNT,
};

enum fmdriver_track_info {
  FMDRIVER_TRACK_INFO_NORMAL,
  FMDRIVER_TRACK_INFO_SSG,
  FMDRIVER_TRACK_INFO_FM3EX,
  FMDRIVER_TRACK_INFO_PPZ8,
  FMDRIVER_TRACK_INFO_PDZF,
  FMDRIVER_TRACK_INFO_SSGEFF,
};

struct fmdriver_track_status {
  bool playing;
  enum fmdriver_track_info info;
  uint8_t ticks;
  uint8_t ticks_left;
  uint8_t key;
  // key after pitchbend, LFO, etc. applied
  uint8_t actual_key;
  uint8_t tonenum;
  uint8_t volume;
  uint8_t gate;
  int8_t detune;
  char status[9];
  bool fmslotmask[4];
  // for FMP, ppz8 channel+1 or 0
  // use for track mask or display
  uint8_t ppz8_ch;
  bool ssg_tone;
  bool ssg_noise;
};

struct fmdriver_work {
  // set by driver, called by opna
  void (*driver_opna_interrupt)(struct fmdriver_work *work);
  void (*driver_deinit)(struct fmdriver_work *work);
  // driver internal
  void *driver;


  // set by opna, called by driver in the interrupt functions
  unsigned (*opna_readreg)(struct fmdriver_work *work, unsigned addr);
  void (*opna_writereg)(struct fmdriver_work *work, unsigned addr, unsigned data);
  uint8_t (*opna_status)(struct fmdriver_work *work, bool a1);
  void *opna;

  const struct ppz8_functbl *ppz8_functbl;
  struct ppz8 *ppz8;

  // if false, 3 line comment
  // if true, PMD memo mode
  bool comment_mode_pmd;

  // CP932 encoded
  // may contain ANSI escape sequences
  // if !comment_mode_pmd:
  //   three lines, 0 <= line < 3
  // if comment_mode_pmd:
  //   line 0: #Title
  //   line 1: #Composer
  //   line 2: #Arranger
  //   line 3: #Memo 1st line
  //      :
  //   line n: NULL
  const char *(*get_comment)(struct fmdriver_work *work, int line);

  // only single-byte uppercase cp932
  char filename[FMDRIVER_TITLE_BUFLEN];

  // PCM: 0    1    2    3
  // PMD: PPC  PPZ1 PPZ2 PPC
  // FMP: PVI  PPZ

  // if (!strlen(pcmtype[i])) this pcm is not available on this driver
  char pcmtype[FMDRIVER_PCMCOUNT][5];
  // CP932 encoded
  char pcmname[FMDRIVER_PCMCOUNT][9];
  // not set by drivers, used by visualizer (FMDSP)
  // set to true when for example the PCM file was not found
  bool pcmerror[FMDRIVER_PCMCOUNT];

  // driver status (for display)
  uint8_t ssg_noise_freq;
  struct fmdriver_track_status track_status[FMDRIVER_TRACK_NUM];
  uint8_t loop_cnt;
  // timerb value
  uint8_t timerb;
  // current timerb count
  uint32_t timerb_cnt;
  // current timerb count, reset on loop
  uint32_t timerb_cnt_loop;
  // loop length, calculated before playing
  uint32_t loop_timerb_cnt;
  // fm3ex part map
  bool playing;
  bool paused;
};

#endif // MYON_FMDRIVER_H_INCLUDED
