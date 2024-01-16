/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
 * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
 * Copyright (C) 2013 by Elias Oenal <EliasOenal@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
 * written because people could not do real time
 * FM demod on Atom hardware with GNU radio
 * based on rtl_sdr.c and rtl_tcp.c
 *
 * lots of locks, but that is okay
 * (no many-to-many locks)
 *
 * todo:
 *       sanity checks
 *       scale squelch to other input parameters
 *       test all the demodulations
 *       pad output on hop
 *       frequency ranges could be stored better
 *       scaled AM demod amplification
 *       auto-hop after time limit
 *       peak detector to tune onto stronger signals
 *       fifo for active hop frequency
 *       clips
 *       noise squelch
 *       merge stereo patch
 *       merge soft agc patch
 *       merge udp patch
 *       testmode to detect overruns
 *       watchdog to reset bad dongle
 *       fix oversampling
 */


#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <alsa/asoundlib.h>

#include <kissfft/kiss_fft.h>
#include <kissfft/kiss_fftr.h>

#include "rtl_fm_lib.h"

/*
   Public Data
*/

struct dongle_state dongle;
struct demod_state demod;
struct output_state output;
struct controller_state controller;

int volatile do_exit;
int ACTUAL_BUF_LENGTH;

/*
   Private Data
*/

static int *atan_lut = NULL;
static int atan_lut_size = 131072; /* 512 KB */
static int atan_lut_coef = 8;

/* {length, coef, coef, coef}  and scaled by 2^15
   for now, only length 9, optimal way to get +85% bandwidth */
#define CIC_TABLE_MAX 10
int cic_9_tables[][10] = {
	{0,},
	{9, -156,  -97, 2798, -15489, 61019, -15489, 2798,  -97, -156},
	{9, -128, -568, 5593, -24125, 74126, -24125, 5593, -568, -128},
	{9, -129, -639, 6187, -26281, 77511, -26281, 6187, -639, -129},
	{9, -122, -612, 6082, -26353, 77818, -26353, 6082, -612, -122},
	{9, -120, -602, 6015, -26269, 77757, -26269, 6015, -602, -120},
	{9, -120, -582, 5951, -26128, 77542, -26128, 5951, -582, -120},
	{9, -119, -580, 5931, -26094, 77505, -26094, 5931, -580, -119},
	{9, -119, -578, 5921, -26077, 77484, -26077, 5921, -578, -119},
	{9, -119, -577, 5917, -26067, 77473, -26067, 5917, -577, -119},
	{9, -199, -362, 5303, -25505, 77489, -25505, 5303, -362, -199},
};

#if defined(_MSC_VER) && (_MSC_VER < 1800)
static double log2(double n)
{
	return log(n) / log(2.0);
}
#endif

static void rotate_90(unsigned char *buf, uint32_t len)
/* 90 rotation is 1+0j, 0+1j, -1+0j, 0-1j
   or [0, 1, -3, 2, -4, -5, 7, -6] */
{
    ////////// 27Mar2023 First Comment Update ///////////////////////
    // https://dsp.stackexchange.com/questions/54950/90-degree-phase-shift-rotation-algorithm-for-sdr
    // 
    // I believe this algorithm isn’t a rotation (phase shift)
    // by 90degrees but instead is a frequency shift of fsamp/4. 
    // In this algorithm each sample is rotated 90 degrees with 
    // respect to the last one causing a frequency shift:
    //    
    //    Samp1: no rotation
    //    Samp2: 90 degree rotation
    //    Samp3: 180 degree rotation
    //    Samp4: 270 degree rotation
    //    Samp5: 360 -> 0 degree rotation
    //    Samp6: 90 degree rotation
    ////////////////////////////////////////////////////////

    ////////// 27Mar2023 2nd Comment Update ///////////////////////
    // https://dsp.stackexchange.com/questions/51889/what-exactly-is-a-90-degree-phase-shift-of-a-digital-signal-in-fm-demodulation-a/51898#51898
    //
    // I want to mention the possibility that this may 
    // actually be a delay element and not a phase shift in 
    // the sense that phase shift implies a shift of that 
    // phase at all frequencies, while a true delay has a 
    // phase shift that is proportional to frequency. It is 
    // this property that is exploited to form simple FM demodulator structures
    //
    // For a pure 90 degree phase shift all you need to do is:
    //
    //    Ir = -Qi, 
    //    Qr = Ii 
    //
    // at every sample.
    ////////////////////////////////////////////////////////

    uint32_t i;
    unsigned char tmp;
    for (i=0; i<len; i+=8) {
        /* uint8_t negation = 255 - x */
        tmp = 255 - buf[i+3];
        buf[i+3] = buf[i+2];
        buf[i+2] = tmp;

        buf[i+4] = 255 - buf[i+4];
        buf[i+5] = 255 - buf[i+5];

        tmp = 255 - buf[i+6];
        buf[i+6] = buf[i+7];
        buf[i+7] = tmp;
    }
}

static void low_pass(struct demod_state *d)
/* simple square window FIR */
{
	int i=0, i2=0;
	while (i < d->lp_len) {
		d->now_r += d->lowpassed[i];
		d->now_j += d->lowpassed[i+1];
		i += 2;
		d->prev_index++;
		if (d->prev_index < d->downsample) {
			continue;
		}
		d->lowpassed[i2]   = d->now_r; // * d->output_scale;
		d->lowpassed[i2+1] = d->now_j; // * d->output_scale;
		d->prev_index = 0;
		d->now_r = 0;
		d->now_j = 0;
		i2 += 2;
	}
	d->lp_len = i2;
}

static int low_pass_simple(int16_t *signal2, int len, int step)
// no wrap around, length must be multiple of step
{
	int i, i2, sum;
	for(i=0; i < len; i+=step) {
		sum = 0;
		for(i2=0; i2<step; i2++) {
			sum += (int)signal2[i + i2];
		}
		//signal2[i/step] = (int16_t)(sum / step);
		signal2[i/step] = (int16_t)(sum);
	}
	signal2[i/step + 1] = signal2[i/step];
	return len / step;
}

static void low_pass_real(struct demod_state *s)
/* simple square window FIR */
// add support for upsampling?
{
	int i=0, i2=0;
	int fast = (int)s->rate_out;
	int slow = s->rate_out2;
	while (i < s->result_len) {
		s->now_lpr += s->result[i];
		i++;
		s->prev_lpr_index += slow;
		if (s->prev_lpr_index < fast) {
			continue;
		}
		s->result[i2] = (int16_t)(s->now_lpr / (fast/slow));
		s->prev_lpr_index -= fast;
		s->now_lpr = 0;
		i2 += 1;
	}
	s->result_len = i2;
}

static void fifth_order(int16_t *data, int length, int16_t *hist)
/* for half of interleaved data */
{
	int i;
	int16_t a, b, c, d, e, f;
	a = hist[1];
	b = hist[2];
	c = hist[3];
	d = hist[4];
	e = hist[5];
	f = data[0];
	/* a downsample should improve resolution, so don't fully shift */
	data[0] = (a + (b+e)*5 + (c+d)*10 + f) >> 4;
	for (i=4; i<length; i+=4) {
		a = c;
		b = d;
		c = e;
		d = f;
		e = data[i-2];
		f = data[i];
		data[i/2] = (a + (b+e)*5 + (c+d)*10 + f) >> 4;
	}
	/* archive */
	hist[0] = a;
	hist[1] = b;
	hist[2] = c;
	hist[3] = d;
	hist[4] = e;
	hist[5] = f;
}

static void generic_fir(int16_t *data, int length, int *fir, int16_t *hist)
/* Okay, not at all generic.  Assumes length 9, fix that eventually. */
{
	int d, temp, sum;
	for (d=0; d<length; d+=2) {
		temp = data[d];
		sum = 0;
		sum += (hist[0] + hist[8]) * fir[1];
		sum += (hist[1] + hist[7]) * fir[2];
		sum += (hist[2] + hist[6]) * fir[3];
		sum += (hist[3] + hist[5]) * fir[4];
		sum +=            hist[4]  * fir[5];
		data[d] = sum >> 15 ;
		hist[0] = hist[1];
		hist[1] = hist[2];
		hist[2] = hist[3];
		hist[3] = hist[4];
		hist[4] = hist[5];
		hist[5] = hist[6];
		hist[6] = hist[7];
		hist[7] = hist[8];
		hist[8] = temp;
	}
}

/* define our own complex math ops
   because ARMv5 has no hardware float */

static void multiply(int ar, int aj, int br, int bj, int *cr, int *cj)
{
	*cr = ar*br - aj*bj;
	*cj = aj*br + ar*bj;
}

static int polar_discriminant(int ar, int aj, int br, int bj)
{
	int cr, cj;
	double angle;
	multiply(ar, aj, br, -bj, &cr, &cj);
	angle = atan2((double)cj, (double)cr);
	return (int)(angle / 3.14159 * (1<<14));
}

static int fast_atan2(int y, int x)
/* pre scaled for int16 */
{
	int yabs, angle;
	int pi4=(1<<12), pi34=3*(1<<12);  // note pi = 1<<14
	if (x==0 && y==0) {
		return 0;
	}
	yabs = y;
	if (yabs < 0) {
		yabs = -yabs;
	}
	if (x >= 0) {
		angle = pi4  - pi4 * (x-yabs) / (x+yabs);
	} else {
		angle = pi34 - pi4 * (x+yabs) / (yabs-x);
	}
	if (y < 0) {
		return -angle;
	}
	return angle;
}

static int polar_disc_fast(int ar, int aj, int br, int bj)
{
	int cr, cj;
	multiply(ar, aj, br, -bj, &cr, &cj);
	return fast_atan2(cj, cr);
}

int atan_lut_init(void)
{
	int i = 0;

	atan_lut = (int*) malloc(atan_lut_size * sizeof(int));

	for (i = 0; i < atan_lut_size; i++) {
		atan_lut[i] = (int) (atan((double) i / (1<<atan_lut_coef)) / 3.14159 * (1<<14));
	}

	return 0;
}

static int polar_disc_lut(int ar, int aj, int br, int bj)
{
	int cr, cj, x, x_abs;

	multiply(ar, aj, br, -bj, &cr, &cj);

	/* special cases */
	if (cr == 0 || cj == 0) {
		if (cr == 0 && cj == 0)
			{return 0;}
		if (cr == 0 && cj > 0)
			{return 1 << 13;}
		if (cr == 0 && cj < 0)
			{return -(1 << 13);}
		if (cj == 0 && cr > 0)
			{return 0;}
		if (cj == 0 && cr < 0)
			{return 1 << 14;}
	}

	/* real range -32768 - 32768 use 64x range -> absolute maximum: 2097152 */
	x = (cj << atan_lut_coef) / cr;
	x_abs = abs(x);

	if (x_abs >= atan_lut_size) {
		/* we can use linear range, but it is not necessary */
		return (cj > 0) ? 1<<13 : -(1<<13);
	}

	if (x > 0) {
		return (cj > 0) ? atan_lut[x] : atan_lut[x] - (1<<14);
	} else {
		return (cj > 0) ? (1<<14) - atan_lut[-x] : -atan_lut[-x];
	}

	return 0;
}

void fm_demod(struct demod_state *fm)
{
	int i, pcm;
	int16_t *lp = fm->lowpassed;
	pcm = polar_discriminant(lp[0], lp[1],
		fm->pre_r, fm->pre_j);
	fm->result[0] = (int16_t)pcm;
	for (i = 2; i < (fm->lp_len-1); i += 2) {
		switch (fm->custom_atan) {
		case 0:
			pcm = polar_discriminant(lp[i], lp[i+1],
				lp[i-2], lp[i-1]);
			break;
		case 1:
			pcm = polar_disc_fast(lp[i], lp[i+1],
				lp[i-2], lp[i-1]);
			break;
		case 2:
			pcm = polar_disc_lut(lp[i], lp[i+1],
				lp[i-2], lp[i-1]);
			break;
		}
		fm->result[i/2] = (int16_t)pcm;
	}
	fm->pre_r = lp[fm->lp_len - 2];
	fm->pre_j = lp[fm->lp_len - 1];
	fm->result_len = fm->lp_len/2;
}

void am_demod(struct demod_state *fm)
// todo, fix this extreme laziness
{
	int i, pcm;
	int16_t *lp = fm->lowpassed;
	int16_t *r  = fm->result;
	for (i = 0; i < fm->lp_len; i += 2) {
		// hypot uses floats but won't overflow
		//r[i/2] = (int16_t)hypot(lp[i], lp[i+1]);
		pcm = lp[i] * lp[i];
		pcm += lp[i+1] * lp[i+1];
		r[i/2] = (int16_t)sqrt(pcm) * fm->output_scale;
	}
	fm->result_len = fm->lp_len/2;
	// lowpass? (3khz)  highpass?  (dc)
}

void usb_demod(struct demod_state *fm)
{
	int i, pcm;
	int16_t *lp = fm->lowpassed;
	int16_t *r  = fm->result;
	for (i = 0; i < fm->lp_len; i += 2) {
		pcm = lp[i] + lp[i+1];
		r[i/2] = (int16_t)pcm * fm->output_scale;
	}
	fm->result_len = fm->lp_len/2;
}

void lsb_demod(struct demod_state *fm)
{
	int i, pcm;
	int16_t *lp = fm->lowpassed;
	int16_t *r  = fm->result;
	for (i = 0; i < fm->lp_len; i += 2) {
		pcm = lp[i] - lp[i+1];
		r[i/2] = (int16_t)pcm * fm->output_scale;
	}
	fm->result_len = fm->lp_len/2;
}

void raw_demod(struct demod_state *fm)
{
	int i;
	for (i = 0; i < fm->lp_len; i++) {
		fm->result[i] = (int16_t)fm->lowpassed[i];
	}
	fm->result_len = fm->lp_len;
}

static void deemph_filter(struct demod_state *fm)
{
	static int avg;  // cheating...
	int i, d;
	// de-emph IIR
	// avg = avg * (1 - alpha) + sample * alpha;
	for (i = 0; i < fm->result_len; i++) {
		d = fm->result[i] - avg;
		if (d > 0) {
			avg += (d + fm->deemph_a/2) / fm->deemph_a;
		} else {
			avg += (d - fm->deemph_a/2) / fm->deemph_a;
		}
		fm->result[i] = (int16_t)avg;
	}
}

static void dc_block_filter(struct demod_state *fm)
{
	int i, avg;
	int64_t sum = 0;
	for (i=0; i < fm->result_len; i++) {
		sum += fm->result[i];
	}
	avg = sum / fm->result_len;
	avg = (avg + fm->dc_avg * 9) / 10;
	for (i=0; i < fm->result_len; i++) {
		fm->result[i] -= avg;
	}
	fm->dc_avg = avg;
}

static int mad(int16_t *samples, int len, int step)
/* mean average deviation */
{
	int i=0, sum=0, ave=0;
	if (len == 0)
		{return 0;}
	for (i=0; i<len; i+=step) {
		sum += samples[i];
	}
	ave = sum / (len * step);
	sum = 0;
	for (i=0; i<len; i+=step) {
		sum += abs(samples[i] - ave);
	}
	return sum / (len / step);
}

// squelch() was written by Jeff
static void squelch(int16_t *samples, int len, int level)
{
        fprintf(stderr, "squelch - Entry - len: %d\n", len);

        static long count = 0;

        int nfft = 4096;
        int nfreqs=nfft/2+1;

        kiss_fftr_cfg   cfg = kiss_fftr_alloc(nfft,0,0,0);
        kiss_fft_scalar *tbuf = (kiss_fft_scalar*) malloc(sizeof(kiss_fft_scalar)*nfft);
        kiss_fft_cpx    *fbuf = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx)*nfreqs);

        int i;
        for (i=0;i<len;++i)  {
            tbuf[i] = samples[i];
        }

        // Add zero-pad samples 
        for (i=len;i<nfft;++i) {
            tbuf[i] = 0;
        }

        // Remove DC bias
        float avg = 0;
        for (i=0;i<nfft;++i)  avg += tbuf[i];
        avg /= nfft;
        for (i=0;i<nfft;++i)  tbuf[i] -= (kiss_fft_scalar)avg;

        // Compute FFT
        kiss_fftr(cfg, tbuf, fbuf);

        // Calc squared magnitude (i.e power)  of complex bin value
        float *mag2buf = (float*) malloc(nfreqs * sizeof(float));
        for (i=0;i<nfreqs;++i) {
            mag2buf[i] += (fbuf[i].r * fbuf[i].r) + (fbuf[i].i * fbuf[i].i);
        }

        fprintf(stderr, "mag2buf[0] = %f\n", mag2buf[0]);

        static int squelch = 0;
        static int unsquelch_cnt = 0;
        if (!squelch) {
            if (mag2buf[0] > level) {
            //if ((mag2buf[0] > 10.0) && (avg_mag2_scaled < 40.0)) {
                squelch = 1;
                unsquelch_cnt = 0;
                printf("MUTE\n");
            }
        } else {
            if (mag2buf[0] < level) {
            //if ((mag2buf[0] < 10.0) && (avg_mag2_scaled > 40.0)) {
                unsquelch_cnt++;
            } else {
                unsquelch_cnt = 0;
            }
            if (unsquelch_cnt == 2) {
                squelch = 0;
                printf("UN-MUTE\n");
            }
        }

        free (cfg);
        free (tbuf);
        free (fbuf);
        free (mag2buf);

        count = (count % INT_MAX) + 1;

	return;
}

// pwr_mean_square_real() was written by Jeff
static uint32_t pwr_mean_square_real(int16_t *samples, int len)
{
        fprintf(stderr, "pwr_mean_square_real - Entry - len: %d\n", len);

        static long count = 0;

	int i;
        uint32_t pms;
	double p, t, s;
	double dc, err;

	p = t = 0.0;
	for (i=0; i<len; ++i) {
		s = (double)samples[i];
                t += s;
		p += s * s;

                if ((count % 10) == 0) {
                    fprintf(stderr, "sample[%d]: %d\n", i, samples[i]);
                }
	}

	/* Calc DC bias by average amplitude */
	dc = t / len;

        // This removal of DC bias by subtracting DC**2 is untested
        pms = (uint32_t) (p - (dc * dc)) / len;

        if (1) {
            fprintf(stderr, "PMS: %d, DC Offset: %f\n", pms, dc);
        }
        count = (count % INT_MAX) + 1;

	return pms;
}

// pwr_mean_square_complex() was written by Jeff
static uint32_t pwr_mean_square_complex(int16_t *samples, int len)
{
        fprintf(stderr, "pwr_mean_square_complex - Entry - len: %d\n", len);

        static long count = 0;

	int32_t i, ti;
	int32_t q, tq;
        uint32_t pms;
	double p, m_sq;
	double dc, err;

        ti = tq = 0;
	p = 0.0;
	int k;
	for (k=0; k<len-1; k=k+2) {
	    i = (int32_t)samples[k];
	    q = (int32_t)samples[k+1];

            m_sq = (double)(i*i) + (double)(q*q);

            ti += i; 
            tq += q; 

	    p += m_sq;

            if ((count % 10) == 0) {
                fprintf(stderr, "sample[%d]: i = %d\tq = %d\n", k, samples[k], samples[k+1]);
            }
	}

	/* Calc DC bias by average amplitude */
	int32_t dci = ti / len;
	int32_t dcq = tq / len;

        // This removal of DC bias by subtracting DC-I**2 plus DC-Q**2 is untested
        pms = (uint32_t)(p - ((dci*dci) + (dcq*dcq))) / len;

        if (1) {
            fprintf(stderr, "PMS: %d, DC Ibias: %d Qbias: %d\n", pms, dci, dcq);
        }
        count = (count % INT_MAX) + 1;

	return pms;
}

static void arbitrary_upsample(int16_t *buf1, int16_t *buf2, int len1, int len2)
/* linear interpolation, len1 < len2 */
{
	int i = 1;
	int j = 0;
	int tick = 0;
	double frac;  // use integers...
	while (j < len2) {
		frac = (double)tick / (double)len2;
		buf2[j] = (int16_t)(buf1[i-1]*(1-frac) + buf1[i]*frac);
		j++;
		tick += len1;
		if (tick > len2) {
			tick -= len2;
			i++;
		}
		if (i >= len1) {
			i = len1 - 1;
			tick = len2;
		}
	}
}

static void arbitrary_downsample(int16_t *buf1, int16_t *buf2, int len1, int len2)
/* fractional boxcar lowpass, len1 > len2 */
{
	int i = 1;
	int j = 0;
	int tick = 0;
	double remainder = 0;
	double frac;  // use integers...
	buf2[0] = 0;
	while (j < len2) {
		frac = 1.0;
		if ((tick + len2) > len1) {
			frac = (double)(len1 - tick) / (double)len2;}
		buf2[j] += (int16_t)((double)buf1[i] * frac + remainder);
		remainder = (double)buf1[i] * (1.0-frac);
		tick += len2;
		i++;
		if (tick > len1) {
			j++;
			buf2[j] = 0;
			tick -= len1;
		}
		if (i >= len1) {
			i = len1 - 1;
			tick = len1;
		}
	}
	for (j=0; j<len2; j++) {
		buf2[j] = buf2[j] * len2 / len1;}
}

static void arbitrary_resample(int16_t *buf1, int16_t *buf2, int len1, int len2)
/* up to you to calculate lengths and make sure it does not go OOB
 * okay for buffers to overlap, if you are downsampling */
{
	if (len1 < len2) {
		arbitrary_upsample(buf1, buf2, len1, len2);
	} else {
		arbitrary_downsample(buf1, buf2, len1, len2);
	}
}

static void full_demod(struct demod_state *d)
{
	int i, ds_p;
	uint32_t sr = 0;
	ds_p = d->downsample_passes;
	if (ds_p) {
		for (i=0; i < ds_p; i++) {
			fifth_order(d->lowpassed,   (d->lp_len >> i), d->lp_i_hist[i]);
			fifth_order(d->lowpassed+1, (d->lp_len >> i) - 1, d->lp_q_hist[i]);
		}
		d->lp_len = d->lp_len >> ds_p;
		/* droop compensation */
		if (d->comp_fir_size == 9 && ds_p <= CIC_TABLE_MAX) {
			generic_fir(d->lowpassed, d->lp_len,
				cic_9_tables[ds_p], d->droop_i_hist);
			generic_fir(d->lowpassed+1, d->lp_len-1,
				cic_9_tables[ds_p], d->droop_q_hist);
		}
	} else {
		low_pass(d);
	}
	d->mode_demod(d);  /* lowpassed -> result */
	if (d->mode_demod == &raw_demod) {
		return;
	}
	/* todo, fm noise squelch */
	// use nicer filter here too?
	if (d->post_downsample > 1) {
		d->result_len = low_pass_simple(d->result, d->result_len, d->post_downsample);}
	if (d->deemph) {
		deemph_filter(d);}
	if (d->dc_block) {
		dc_block_filter(d);}
	if (d->rate_out2 > 0) {
		low_pass_real(d);
		//arbitrary_resample(d->result, d->result, d->result_len, d->result_len * d->rate_out2 / d->rate_out);
	}
	if (d->squelch_level > 0) {
            //squelch(d->result, d->result_len, d->squelch_level);
	}
}

static int get_bool_simple(char **ptr, char *str, int invert, int orig)
{
	if (**ptr == ':')
		(*ptr)++;
	if (!strncasecmp(*ptr, str, strlen(str))) {
		orig = 1 ^ (invert ? 1 : 0);
		while (**ptr != '\0' && **ptr != ',' && **ptr != ':')
			(*ptr)++;
	}
	if (**ptr == ',' || **ptr == ':')
		(*ptr)++;
	return orig;
}

static void set_mixer(char* control) {

    // more /proc/asound/cards
    //     0 [audioinjectorpi]: audioinjector-p - audioinjector-pi-soundcard
    //                          audioinjector-pi-soundcard
    //
    // more /proc/asound/devices 
    //         0: [ 0]   : control
    //        16: [ 0- 0]: digital audio playback
    //        24: [ 0- 0]: digital audio capture
    //        33:        : timer
    //
    // more /proc/asound/modules 
    //        0 (efault)
    //
    // more /proc/asound/card0/id
    //        audioinjectorpi
    //
    // more /proc/asound/card0/pcm0p/info 
    //        card: 0
    //        device: 0
    //        subdevice: 0
    //        stream: PLAYBACK
    //        id: AudioInjector audio wm8731-hifi-0
    //        name: AudioInjector audio wm8731-hifi-0
    //        subname: subdevice #0
    //        class: 0
    //        subclass: 0
    //        subdevices_count: 1
    //        subdevices_avail: 1

    snd_mixer_elem_t*              elem; // initialize to ???
    snd_mixer_selem_channel_id_t   chn;  // 0 to SND_MIXER_SCHN_LAST
    int                            ival;

    if (!strncmp(control, "mute", 4) &&   snd_mixer_selem_has_playback_switch(elem)) {
        snd_mixer_selem_get_playback_switch(elem, chn, &ival);
        if (snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&control, control, 1, ival)) >= 0)
            fprintf(stderr, "set_mixer(): mute ON\n");
        else
            fprintf(stderr, "set_mixer(): mute not set\n");

    } else if (!strncmp(control, "unmute", 6) && snd_mixer_selem_has_playback_switch(elem)) {
        snd_mixer_selem_get_playback_switch(elem, chn, &ival);
        if (snd_mixer_selem_set_playback_switch(elem, chn, get_bool_simple(&control, control, 0, ival)) >= 0)
            fprintf(stderr, "set_mixer(): mute OFF\n");
        else
            fprintf(stderr, "set_mixer(): un-mute not set\n");
    }
}

static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	int i;
	struct dongle_state *s = (dongle_state*) ctx;
	struct demod_state *d = s->demod_target;

	if (do_exit) {
		return;}
	if (!ctx) {
		return;}
	if (s->mute) {
		for (i=0; i<s->mute; i++) {
			buf[i] = 127;}
		s->mute = 0;
	}
	if (!s->offset_tuning) {
		rotate_90(buf, len);}
	for (i=0; i<(int)len; i++) {
		s->buf16[i] = (int16_t)buf[i] - 127;}
	pthread_rwlock_wrlock(&d->rw);
	memcpy(d->lowpassed, s->buf16, 2*len);
	d->lp_len = len;
	pthread_rwlock_unlock(&d->rw);
	safe_cond_signal(&d->ready, &d->ready_m);
}

void *dongle_thread_fn(void *arg)
{
        fprintf(stderr, "dongle TID: %lu\n", gettid());

	struct dongle_state *s = (dongle_state*) arg;
	rtlsdr_read_async(s->dev, rtlsdr_callback, s, 0, s->buf_len);
	return 0;
}

void *demod_thread_fn(void *arg)
{
        fprintf(stderr, "demod TID: %lu\n", gettid());

	struct demod_state *d = (demod_state*) arg;
	struct output_state *o = d->output_target;
	while (!do_exit) {
		safe_cond_wait(&d->ready, &d->ready_m);
		pthread_rwlock_wrlock(&d->rw);
		full_demod(d);
		pthread_rwlock_unlock(&d->rw);
		if (d->exit_flag) {
			do_exit = 1;
		}
		//if (this block was squelched) {
		//	continue;  // don't output
		//}
		pthread_rwlock_wrlock(&o->rw);
		memcpy(o->result, d->result, 2*d->result_len);
		o->result_len = d->result_len;
		pthread_rwlock_unlock(&o->rw);
		safe_cond_signal(&o->ready, &o->ready_m);
	}
	return 0;
}

void *output_thread_fn(void *arg)
{
        fprintf(stderr, "output TID: %lu\n", gettid());

	struct output_state *s = (output_state*) arg;
	while (!do_exit) {
		// use timedwait and pad out under runs
		safe_cond_wait(&s->ready, &s->ready_m);
		pthread_rwlock_rdlock(&s->rw);
		fwrite(s->result, 2, s->result_len, s->file);
		pthread_rwlock_unlock(&s->rw);
	}
	return 0;
}

void optimal_settings(int freq, int rate)
{
	// giant ball of hacks
	// seems unable to do a single pass, 2:1
	int capture_freq, capture_rate;
	struct dongle_state *d = &dongle;
	struct demod_state *dm = &demod;
	struct controller_state *cs = &controller;
	dm->downsample = (1000000 / dm->rate_in) + 1;
	if (dm->downsample_passes) {
		dm->downsample_passes = (int)log2(dm->downsample) + 1;
		dm->downsample = 1 << dm->downsample_passes;
	}
	capture_freq = freq;
	capture_rate = dm->downsample * dm->rate_in;

	//if (cs->wb_mode) {
	//	capture_freq += 16000;}
	if (!d->offset_tuning) {
		capture_freq += capture_rate/4;}
	capture_freq += cs->edge * dm->rate_in / 2;
	dm->output_scale = (1<<15) / (128 * dm->downsample);
	if (dm->output_scale < 1) {
		dm->output_scale = 1;}
	if (dm->mode_demod == &fm_demod) {
		dm->output_scale = 1;}
	d->freq = (uint32_t)capture_freq;
	d->rate = (uint32_t)capture_rate;
}

void *controller_thread_fn(void *arg)
{
        fprintf(stderr, "controller TID: %lu\n", gettid());

	// thoughts for multiple dongles
	// might be no good using a controller thread if retune/rate blocks
	int i;
	struct controller_state *s = (controller_state*) arg;

	/* set up primary channel */
        fprintf(stderr, "FM Dial Center Freq (MHz) : %f\n", (float)s->freqs[0] * 1e-6);
	optimal_settings(s->freqs[0], demod.rate_in);
	if (dongle.direct_sampling) {
		verbose_direct_sampling(dongle.dev, 1);}
	if (dongle.offset_tuning) {
		verbose_offset_tuning(dongle.dev);}

	/* Set the frequency */

	verbose_set_frequency(dongle.dev, dongle.freq);
	fprintf(stderr, "Oversampling input by: %ix.\n", demod.downsample);
	fprintf(stderr, "Oversampling output by: %ix.\n", demod.post_downsample);
	fprintf(stderr, "Buffer size: %0.2fms\n",
		1000 * 0.5 * (float)ACTUAL_BUF_LENGTH / (float)dongle.rate);

	/* Set the sample rate */
	verbose_set_sample_rate(dongle.dev, dongle.rate);
	fprintf(stderr, "Output at %u Hz.\n", demod.rate_in/demod.post_downsample);

	while (!do_exit) {
		safe_cond_wait(&s->hop, &s->hop_m);
		if (s->freq_len <= 1) {
			continue;}
		/* hacky hopping */
		s->freq_now = (s->freq_now + 1) % s->freq_len;
		optimal_settings(s->freqs[s->freq_now], demod.rate_in);
		rtlsdr_set_center_freq(dongle.dev, dongle.freq);
		dongle.mute = BUFFER_DUMP;
	}
	return 0;
}

void frequency_range(struct controller_state *s, char *arg)
{
	char *start, *stop, *step;
	int i;
	start = arg;
	stop = strchr(start, ':') + 1;
	stop[-1] = '\0';
	step = strchr(stop, ':') + 1;
	step[-1] = '\0';
	for(i=(int)atofs(start); i<=(int)atofs(stop); i+=(int)atofs(step))
	{
		s->freqs[s->freq_len] = (uint32_t)i;
		s->freq_len++;
		if (s->freq_len >= FREQUENCIES_LIMIT) {
			break;}
	}
	stop[-1] = ':';
	step[-1] = ':';
}

void dongle_init(struct dongle_state *s)
{
	s->rate = DEFAULT_SAMPLE_RATE;
	s->gain = AUTO_GAIN; // tenths of a dB
	s->mute = 0;
	s->direct_sampling = 0;
	s->offset_tuning = 0;
	s->demod_target = &demod;
}

void demod_init(struct demod_state *s)
{
	s->rate_in = DEFAULT_SAMPLE_RATE;
	s->rate_out = DEFAULT_SAMPLE_RATE;
	s->squelch_level = 0;
	s->downsample_passes = 0;
	s->comp_fir_size = 0;
	s->prev_index = 0;
	s->post_downsample = 1;  // once this works, default = 4
	s->custom_atan = 0;
	s->deemph = 0;
	s->rate_out2 = -1;  // flag for disabled
	s->mode_demod = &fm_demod;
	s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
	s->prev_lpr_index = 0;
	s->deemph_a = 0;
	s->now_lpr = 0;
	s->dc_block = 0;
	s->dc_avg = 0;
	pthread_rwlock_init(&s->rw, NULL);
	pthread_cond_init(&s->ready, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
	s->output_target = &output;
}

void demod_cleanup(struct demod_state *s)
{
	pthread_rwlock_destroy(&s->rw);
	pthread_cond_destroy(&s->ready);
	pthread_mutex_destroy(&s->ready_m);
}

void output_init(struct output_state *s)
{
	s->rate = DEFAULT_SAMPLE_RATE;
	pthread_rwlock_init(&s->rw, NULL);
	pthread_cond_init(&s->ready, NULL);
	pthread_mutex_init(&s->ready_m, NULL);
}

void output_cleanup(struct output_state *s)
{
	pthread_rwlock_destroy(&s->rw);
	pthread_cond_destroy(&s->ready);
	pthread_mutex_destroy(&s->ready_m);
}

void controller_init(struct controller_state *s)
{
	s->freqs[0] = 100000000;
	s->freq_len = 0;
	s->edge = 0;
	s->wb_mode = 0;
	pthread_cond_init(&s->hop, NULL);
	pthread_mutex_init(&s->hop_m, NULL);
}

void controller_cleanup(struct controller_state *s)
{
	pthread_cond_destroy(&s->hop);
	pthread_mutex_destroy(&s->hop_m);
}

void sanity_checks(void)
{
	if (controller.freq_len == 0) {
		fprintf(stderr, "Please specify a frequency.\n");
		exit(1);
	}

	if (controller.freq_len >= FREQUENCIES_LIMIT) {
		fprintf(stderr, "Too many channels, maximum %i.\n", FREQUENCIES_LIMIT);
		exit(1);
	}

	if (controller.freq_len > 1 && demod.squelch_level == 0) {
		fprintf(stderr, "Please specify a squelch level.  Required for scanning multiple frequencies.\n");
		exit(1);
	}

}

