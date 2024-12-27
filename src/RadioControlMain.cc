/*

Read the Clock-Wise and Counter-Clock-Wise state 
changes of an incremental rotary encoder using 
the sysfs interface on a Raspberry Pi.


Jeff McLane <jkmclane68@yahoo.com>

*/

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>

#include "QueueThreadSafe.hh"
#include "RotaryEncoderEvent.hh"
//#include "LcdI2cHD44780.hh"
#include "OledI2cSH1106.hh"
#include "rtl_fm_lib.h"
#include "RadioControlMain.hh"

RadioControlMain::RadioControlMain() :
    m_rotary_encoder(NULL),
    m_tune_queue(NULL)
{
}

RadioControlMain::~RadioControlMain() {

    int* thread_stat;
    pthread_join(m_rotary_encoder_thread, (void**) &thread_stat);
    fprintf(stderr, "RadioControlMain::~RadioControlMain : joined with m_rotary_encoder_thread\n");

    if (m_rotary_encoder != NULL) delete m_rotary_encoder;
    if (m_tune_queue != NULL) delete m_tune_queue;
}

bool RadioControlMain::init() {

    #ifdef _WIN32
    SetConsoleCtrlHandler( (PHANDLER_ROUTINE) sighandler, TRUE );
    #else
    struct sigaction sigact;
    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);
    #endif

    // Initialize the list of FM radio center frequencies

    double freq_MHz = 87.5;
    //double freq_MHz = 90.1;
    m_stn_idx = 0;
    for (int ii=0; ii<NUM_FM_FREQS; ++ii) {

        m_fm_center_freqs_MHz[ii] = freq_MHz;
        freq_MHz += 0.200; // 200 kHz spacing
    }

    // Initialize the RTL-SDR tuned frequency

    controller.freqs[controller.freq_len] = m_fm_center_freqs_MHz[m_stn_idx] * 1e6;
    optimal_settings(controller.freqs[controller.freq_len], demod.rate_in);
    controller.freq_len++;

    //
    // Initialize and start a thread to monitor the radio frequency dial
    //

    m_tune_queue = new QueueThreadSafe<RotaryEncoderEvent::RotaryStates>(do_exit);

    m_rotary_encoder = new RotaryEncoderEvent(&do_exit, m_tune_queue);
    bool rotary_encoder_status = m_rotary_encoder->init();
    m_rotary_encoder_thread = m_rotary_encoder->run();

    //
    // Initialize the radio frequency display
    //

    m_lcd.init();
    m_lcd.clrLcd();
    m_lcd.typeln("RF (MHz): ");
    m_lcd.typeFloat(m_fm_center_freqs_MHz[m_stn_idx]);

    return true;
}

void RadioControlMain::wait_for_frequency_change() {

    fprintf(stderr, "RadioControlMain::wait_for_frequency_change : waiting to pop\n");
    RotaryEncoderEvent::RotaryStates rs = m_tune_queue->pop();
    fprintf(stderr, "RadioControlMain::wait_for_frequency_change : popped %d\n", rs);

    switch(rs) {
        case RotaryEncoderEvent::ROT_INCREMENT:
        case RotaryEncoderEvent::ROT_SW_PUSHED:
        {
            m_stn_idx = (m_stn_idx + rs) % NUM_FM_FREQS ;
            break;
        }
        case RotaryEncoderEvent::ROT_DECREMENT:
        {
            (m_stn_idx == 0) ? m_stn_idx = (NUM_FM_FREQS + rs) : m_stn_idx += rs;
            break;
        }
        default:
        {
            std::cerr << "Unexpected RotaryEncoderEvent::RotaryStates: " << rs << std::endl;
            break;
        }
    }

    std::cerr << "RadioControlMain::wait_for_frequency_change: frequency change detected : " << rs << std::endl;

    char center_freq_MHz_s[10];
    sprintf(center_freq_MHz_s, "%5.1f", m_fm_center_freqs_MHz[m_stn_idx]);

    std::cerr << "FM Dial Center Freq (MHz) : " <<  m_fm_center_freqs_MHz[m_stn_idx] << " / " << center_freq_MHz_s << std::endl;

    //
    // Update the frequency
    //

    // In the LCD display
    m_lcd.lcdLoc(LINE1);
    m_lcd.typeln("RF (MHz): ");
    m_lcd.typeFloat(m_fm_center_freqs_MHz[m_stn_idx]);

    // In the RTL-SDR dongle
    int freq_Hz = (int) (m_fm_center_freqs_MHz[m_stn_idx] * 1e6);
    optimal_settings(freq_Hz, demod.rate_in);
    verbose_set_frequency(dongle.dev, dongle.freq);

    return;
}

#ifdef _WIN32
BOOL WINAPI RadioControlMain::sighandler(int signum)
{
    if (CTRL_C_EVENT == signum) {
        fprintf(stderr, "Signal caught, exiting!\n");
        do_exit = 1;
        return TRUE;
    }
    return FALSE;
}
#else
void RadioControlMain::sighandler(int signum)
{
    fprintf(stderr, "Signal caught, exiting!\n");
    do_exit = 1;
}
#endif

void usage(void)
{
        fprintf(stderr,
                "rtl_fm, a simple narrow band FM demodulator for RTL2832 based DVB-T receivers\n\n"
                "Use:\trtl_fm -f freq [-options] [filename]\n"
                "\t-f frequency_to_tune_to [Hz]\n"
                "\t    use multiple -f for scanning (requires squelch)\n"
                "\t    ranges supported, -f 118M:137M:25k\n"
                "\t[-M modulation (default: fm)]\n"
                "\t    fm, wbfm, raw, am, usb, lsb\n"
                "\t    wbfm == -M fm -s 170k -o 4 -A fast -r 32k -l 0 -E deemp\n"
                "\t    raw mode outputs 2x16 bit IQ pairs\n"
                "\t[-s sample_rate (default: 24k)]\n"
                "\t[-d device_index (default: 0)]\n"
                "\t[-T enable bias-T on GPIO PIN 0 (works for rtl-sdr.com v3 dongles)]\n"
                "\t[-g tuner_gain (default: automatic)]\n"
                "\t[-l squelch_level (default: 0/off)]\n"
                //"\t    for fm squelch is inverted\n"
                //"\t[-o oversampling (default: 1, 4 recommended)]\n"
                "\t[-p ppm_error (default: 0)]\n"
                "\t[-E enable_option (default: none)]\n"
                "\t    use multiple -E to enable multiple options\n"
                "\t    edge:   enable lower edge tuning\n"
                "\t    dc:     enable dc blocking filter\n"
                "\t    deemp:  enable de-emphasis filter\n"
                "\t    direct: enable direct sampling\n"
                "\t    offset: enable offset tuning\n"
                "\tfilename ('-' means stdout)\n"
                "\t    omitting the filename also uses stdout\n\n"
                "Experimental options:\n"
                "\t[-r resample_rate (default: none / same as -s)]\n"
                "\t    +values will mute/scan, -values will exit\n"
                "\t[-F fir_size (default: off)]\n"
                "\t    enables low-leakage downsample filter\n"
                "\t    size can be 0 or 9.  0 has bad roll off\n"
                "\t[-A std/fast/lut choose atan math (default: std)]\n"
                //"\t[-C clip_path (default: off)\n"
                //"\t (create time stamped raw clips, requires squelch)\n"
                //"\t (path must have '\%s' and will expand to date_time_freq)\n"
                //"\t[-H hop_fifo (default: off)\n"
                //"\t (fifo will contain the active frequency)\n"
                "\n"
                "Produces signed 16 bit ints, use Sox or aplay to hear them.\n"
                "\trtl_fm ... | play -t raw -r 24k -es -b 16 -c 1 -V1 -\n"
                "\t           | aplay -r 24k -f S16_LE -t raw -c 1\n"
                "\t  -M wbfm  | play -r 32k ... \n"
                "\t  -s 22050 | multimon -t raw /dev/stdin\n\n");
        exit(1);
}

int main(int argc, char **argv)
{
    //
    // BEGIN - From the repurposed main() from 'rtl_fm.c'
    //
    dongle_init(&dongle);
    demod_init(&demod);
    output_init(&output);
    controller_init(&controller);

    int r, opt;
    int dev_given = 0;
    int custom_ppm = 0;
    int enable_biastee = 0;

    while ((opt = getopt(argc, argv, "d:f:g:s:b:l:o:t:r:p:E:F:A:M:hT")) != -1) {
        switch (opt) {
        case 'd':
            dongle.dev_index = verbose_device_search(optarg);
            dev_given = 1;
            break;
        case 'f':
            if (controller.freq_len >= FREQUENCIES_LIMIT) {
                break;
            }
            if (strchr(optarg, ':')) {
                frequency_range(&controller, optarg);
            }
            else {
                controller.freqs[controller.freq_len] = (uint32_t)atofs(optarg);
                controller.freq_len++;
            }
            break;
        case 'g':
            dongle.gain = (int)(atof(optarg) * 10);
            break;
        case 'l':
            demod.squelch_level = (int)atof(optarg);
            break;
        case 's':
            demod.rate_in = (uint32_t)atofs(optarg);
            demod.rate_out = (uint32_t)atofs(optarg);
            break;
        case 'r':
            output.rate = (int)atofs(optarg);
            demod.rate_out2 = (int)atofs(optarg);
            break;
        case 'o':
            fprintf(stderr, "Warning: -o is very buggy\n");
            demod.post_downsample = (int)atof(optarg);
            if (demod.post_downsample < 1 || demod.post_downsample >> MAXIMUM_OVERSAMPLE) {
                fprintf(stderr, "Oversample must be between 1 and %i\n", MAXIMUM_OVERSAMPLE);
            }
            break;
        case 'p':
            dongle.ppm_error = atoi(optarg);
            custom_ppm = 1;
            break;
        case 'E':
            if (strcmp("edge",  optarg) == 0) {
                controller.edge = 1;
            }
            if (strcmp("dc", optarg) == 0) {
                demod.dc_block = 1;
            }
            if (strcmp("deemp",  optarg) == 0) {
                demod.deemph = 1;
            }
            if (strcmp("direct",  optarg) == 0) {
                dongle.direct_sampling = 1;
            }
            if (strcmp("offset",  optarg) == 0) {
                dongle.offset_tuning = 1;
            }
            break;
        case 'F':
            demod.downsample_passes = 1;  /* truthy placeholder */
            demod.comp_fir_size = atoi(optarg);
            break;
        case 'A':
            if (strcmp("std",  optarg) == 0) {
                demod.custom_atan = 0;
            }
            if (strcmp("fast", optarg) == 0) {
                demod.custom_atan = 1;
            }
            if (strcmp("lut",  optarg) == 0) {
                atan_lut_init();
                demod.custom_atan = 2;
            }
            break;
        case 'M':
            if (strcmp("fm",  optarg) == 0) {
                demod.mode_demod = &fm_demod;
            }
            if (strcmp("raw",  optarg) == 0) {
                demod.mode_demod = &raw_demod;
            }
            if (strcmp("am",  optarg) == 0) {
                demod.mode_demod = &am_demod;
            }
            if (strcmp("usb", optarg) == 0) {
                demod.mode_demod = &usb_demod;
            }
            if (strcmp("lsb", optarg) == 0) {
                demod.mode_demod = &lsb_demod;
            }
            if (strcmp("wbfm",  optarg) == 0) {
                controller.wb_mode = 1;
                demod.mode_demod = &fm_demod;
                demod.rate_in = 170000;
                demod.rate_out = 170000;
                demod.rate_out2 = 32000;
                demod.custom_atan = 1;
                //demod.post_downsample = 4;
                demod.deemph = 1;
            }
            break;
        case 'T':
            enable_biastee = 1;
            break;
        case 'h':
        default:
            usage();
            break;
        }
    }

    /* quadruple sample_rate to limit to ��θto ��/2 */
    demod.rate_in *= demod.post_downsample;

    if (!output.rate) {
        output.rate = demod.rate_out;
    }

    if (argc <= optind) {
        output.filename = "-";
    }
    else {
        output.filename = argv[optind];
    }

    int lcm_post[17] = {1,1,1,3,1,5,3,7,1,9,5,11,3,13,7,15,1};
    ACTUAL_BUF_LENGTH = lcm_post[demod.post_downsample] * DEFAULT_BUF_LENGTH;

    if (!dev_given) {
        char* dev = (char*) malloc(2);
        strcpy(dev, "0");
        dongle.dev_index = verbose_device_search(dev);
        free(dev);
    }

    if (dongle.dev_index < 0) {
        exit(1);
    }

    r = rtlsdr_open(&dongle.dev, (uint32_t)dongle.dev_index);
    if (r < 0) {
        fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dongle.dev_index);
        exit(1);
    }

    if (demod.deemph) {
        demod.deemph_a = (int)round(1.0/((1.0-exp(-1.0/(demod.rate_out * 75e-6)))));
    }

    /* Set the tuner gain */
    if (dongle.gain == AUTO_GAIN) {
        verbose_auto_gain(dongle.dev);
    } else {
        dongle.gain = nearest_gain(dongle.dev, dongle.gain);
        verbose_gain_set(dongle.dev, dongle.gain);
    }

    rtlsdr_set_bias_tee(dongle.dev, enable_biastee);
    if (enable_biastee) {
        fprintf(stderr, "activated bias-T on GPIO PIN 0\n");
    }

    // NEW -- Create and initialize the Radio Controller

    RadioControlMain rcm;
    rcm.init();

    // END NEW -- Create and initialize the Radio Controller

    sanity_checks();

    verbose_ppm_set(dongle.dev, dongle.ppm_error);

    if (strcmp(output.filename, "-") == 0) { /* Write samples to stdout */
        output.file = stdout;
#ifdef _WIN32
        _setmode(_fileno(output.file), _O_BINARY);
#endif
    } else {
        output.file = fopen(output.filename, "wb");
        if (!output.file) {
            fprintf(stderr, "Failed to open %s\n", output.filename);
            exit(1);
        }
    }

    //r = rtlsdr_set_testmode(dongle.dev, 1);

    /* Reset endpoint before we start reading from it (mandatory) */
    verbose_reset_buffer(dongle.dev);

    fprintf(stderr, "main: TID: %lu\n", gettid());
    pthread_create(&controller.thread, NULL, controller_thread_fn, (void *)(&controller));
    usleep(100000);
    pthread_create(&output.thread, NULL, output_thread_fn, (void *)(&output));
    pthread_create(&demod.thread, NULL, demod_thread_fn, (void *)(&demod));
    pthread_create(&dongle.thread, NULL, dongle_thread_fn, (void *)(&dongle));

    //
    // END - From the repurposed main() from 'rtl_fm.c'
    //

    // Do forever

    do_exit = 0;
    while(!do_exit) {

        // Wait for asynchronous frequency changes
        rcm.wait_for_frequency_change();

    } // End While

    //
    // BEGIN - From the repurposed main() from 'rtl_fm.c'
    //
    if (do_exit) {
        fprintf(stderr, "\nUser cancel, exiting...\n");
    }
    else {
        fprintf(stderr, "\nLibrary error %d, exiting...\n", r);
    }

    //rtlsdr_cancel_async(dongle.dev); // this redundant invocation was removed from sighandler()
    rtlsdr_cancel_async(dongle.dev);
    pthread_join(dongle.thread, NULL);
    safe_cond_signal(&demod.ready, &demod.ready_m);
    pthread_join(demod.thread, NULL);
    safe_cond_signal(&output.ready, &output.ready_m);
    pthread_join(output.thread, NULL);
    safe_cond_signal(&controller.hop, &controller.hop_m);
    pthread_join(controller.thread, NULL);

    //dongle_cleanup(&dongle);
    demod_cleanup(&demod);
    output_cleanup(&output);
    controller_cleanup(&controller);

    if (output.file != stdout) {
        fclose(output.file);
    }

    rtlsdr_close(dongle.dev);
    return r >= 0 ? r : -r;

    //
    // END - From the repurposed main() from 'rtl_fm.c'
    //
}
