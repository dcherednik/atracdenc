/*
 * ATRAC3+ compatible decoder
 *
 * Copyright (c) 2010-2013 Maxim Poliakovski
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 *  @file
 *  DSP functions for ATRAC3+ decoder.
 */

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

#include "atrac3plus.h"

#define TWOPI (2 * M_PI)
#define DEQUANT_PHASE(ph) (((ph) & 0x1F) << 6)

static float sine_table[2048]; ///< wave table
static float hann_window[256]; ///< Hann windowing function
static float amp_sf_tab[64];   ///< scalefactors for quantized amplitudes

static void vector_fmul(float *dst, const float *src0, const float *src1,
                          int len)
{
    int i;
    for (i = 0; i < len; i++)
        dst[i] = src0[i] * src1[i];
}

void ff_atrac3p_init_dsp_static(void)
{
    int i;

    /* generate sine wave table */
    for (i = 0; i < 2048; i++)
        sine_table[i] = sin(TWOPI * i / 2048);

    /* generate Hann window */
    for (i = 0; i < 256; i++)
        hann_window[i] = (1.0f - cos(TWOPI * i / 256.0f)) * 0.5f;

    /* generate amplitude scalefactors table */
    for (i = 0; i < 64; i++)
        amp_sf_tab[i] = exp2f((i - 3) / 4.0f);
}

/**
 *  Synthesize sine waves according to given parameters.
 *
 *  @param[in]    synth_param   ptr to common synthesis parameters
 *  @param[in]    waves_info    parameters for each sine wave
 *  @param[in]    envelope      envelope data for all waves in a group
 *  @param[in]    invert_phase  flag indicating 180° phase shift
 *  @param[in]    reg_offset    region offset for trimming envelope data
 *  @param[out]   out           receives sythesized data
 */

static void waves_synth(Atrac3pWaveSynthParams *synth_param,
                        Atrac3pWavesData *waves_info,
                        Atrac3pWaveEnvelope *envelope,
                        int invert_phase, int reg_offset, float *out)
{
    int i, wn, inc, pos;
    double amp;
    Atrac3pWaveParam *wave_param = &synth_param->waves[waves_info->start_index];

    for (wn = 0; wn < waves_info->num_wavs; wn++, wave_param++) {
        /* amplitude dequantization */
        amp = amp_sf_tab[wave_param->amp_sf] *
              (!synth_param->amplitude_mode
               ? (wave_param->amp_index + 1) / 15.13f
               : 1.0f);

        inc = wave_param->freq_index;
        pos = DEQUANT_PHASE(wave_param->phase_index) - (reg_offset ^ 128) * inc & 2047;

        /* waveform generation */
        for (i = 0; i < 128; i++) {
            out[i] += sine_table[pos] * amp;
            pos     = (pos + inc) & 2047;
        }
    }

    /* invert phase if requested */
    if (invert_phase)
        for (i = 0; i < 128; i++)
            out[i] *= -1.0f;

    /* fade in with steep Hann window if requested */
    if (envelope->has_start_point) {
        pos = (envelope->start_pos << 2) - reg_offset;
        if (pos > 0 && pos <= 128) {
            memset(out, 0, pos * sizeof(*out));
            if (!envelope->has_stop_point ||
                envelope->start_pos != envelope->stop_pos) {
                out[pos + 0] *= hann_window[0];
                out[pos + 1] *= hann_window[32];
                out[pos + 2] *= hann_window[64];
                out[pos + 3] *= hann_window[96];
            }
        }
    }

    /* fade out with steep Hann window if requested */
    if (envelope->has_stop_point) {
        pos = (envelope->stop_pos + 1 << 2) - reg_offset;
        if (pos > 0 && pos <= 128) {
            out[pos - 4] *= hann_window[96];
            out[pos - 3] *= hann_window[64];
            out[pos - 2] *= hann_window[32];
            out[pos - 1] *= hann_window[0];
            memset(&out[pos], 0, (128 - pos) * sizeof(out[pos]));
        }
    }
}

void ff_atrac3p_generate_tones(Atrac3pChanUnitCtx *ch_unit,
                               int ch_num, int sb, float *out)
{
    float wavreg1[128] = { 0 };
    float wavreg2[128] = { 0 };
    int i, reg1_env_nonzero, reg2_env_nonzero;
    Atrac3pWavesData *tones_now  = &ch_unit->channels[ch_num].tones_info_prev[sb];
    Atrac3pWavesData *tones_next = &ch_unit->channels[ch_num].tones_info[sb];

    /* reconstruct full envelopes for both overlapping regions
     * from truncated bitstream data */
    if (tones_next->pend_env.has_start_point &&
        tones_next->pend_env.start_pos < tones_next->pend_env.stop_pos) {
        tones_next->curr_env.has_start_point = 1;
        tones_next->curr_env.start_pos       = tones_next->pend_env.start_pos + 32;
    } else if (tones_now->pend_env.has_start_point) {
        tones_next->curr_env.has_start_point = 1;
        tones_next->curr_env.start_pos       = tones_now->pend_env.start_pos;
    } else {
        tones_next->curr_env.has_start_point = 0;
        tones_next->curr_env.start_pos       = 0;
    }

    if (tones_now->pend_env.has_stop_point &&
        tones_now->pend_env.stop_pos >= tones_next->curr_env.start_pos) {
        tones_next->curr_env.has_stop_point = 1;
        tones_next->curr_env.stop_pos       = tones_now->pend_env.stop_pos;
    } else if (tones_next->pend_env.has_stop_point) {
        tones_next->curr_env.has_stop_point = 1;
        tones_next->curr_env.stop_pos       = tones_next->pend_env.stop_pos + 32;
    } else {
        tones_next->curr_env.has_stop_point = 0;
        tones_next->curr_env.stop_pos       = 64;
    }

    /* is the visible part of the envelope non-zero? */
    reg1_env_nonzero = (tones_now->curr_env.stop_pos    < 32) ? 0 : 1;
    reg2_env_nonzero = (tones_next->curr_env.start_pos >= 32) ? 0 : 1;

    /* synthesize waves for both overlapping regions */
    if (tones_now->num_wavs && reg1_env_nonzero) {
        waves_synth(ch_unit->waves_info_prev, tones_now, &tones_now->curr_env,
                    ch_unit->waves_info_prev->invert_phase[sb] & ch_num,
                    128, wavreg1);
    }

    if (tones_next->num_wavs && reg2_env_nonzero) {
        waves_synth(ch_unit->waves_info, tones_next, &tones_next->curr_env,
                    ch_unit->waves_info->invert_phase[sb] & ch_num, 0, wavreg2);
    }

    /* Hann windowing for non-faded wave signals */
    if (tones_now->num_wavs && tones_next->num_wavs &&
        reg1_env_nonzero && reg2_env_nonzero) {
        vector_fmul(wavreg1, wavreg1, &hann_window[128], 128);
        vector_fmul(wavreg2, wavreg2,  hann_window,      128);
    } else {
        if (tones_now->num_wavs && !tones_now->curr_env.has_stop_point)
            vector_fmul(wavreg1, wavreg1, &hann_window[128], 128);

        if (tones_next->num_wavs && !tones_next->curr_env.has_start_point)
            vector_fmul(wavreg2, wavreg2, hann_window, 128);
    }

    /* Overlap and subtract to get residual */
    for (i = 0; i < 128; i++) {
        out[i] -= wavreg1[i] + wavreg2[i];
    }
}
