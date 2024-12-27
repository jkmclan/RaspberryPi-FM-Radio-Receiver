/*

Jeff McLane <jkmclane68@yahoo.com>

*/

//
// Forward Declarations
//

template <typename T>
class QueueThreadSafe;

//
// Class Declaration
//

class RadioControlMain {

public:

    RadioControlMain();

    ~RadioControlMain();

    bool init();

    void wait_for_frequency_change();

protected:

private:

    #ifdef _WIN32
    static BOOL WINAPI sighandler(int signum);
    #else
    static void sighandler(int signum);
    #endif

    QueueThreadSafe<RotaryEncoderEvent::RotaryStates>* m_tune_queue;

    RotaryEncoderEvent*  m_rotary_encoder;
    pthread_t            m_rotary_encoder_thread;

    //LcdI2cHD44780 m_lcd;
    OledI2cSH1106 m_lcd;

    static const int NUM_FM_FREQS = 103;

    double m_fm_center_freqs_MHz[NUM_FM_FREQS];

    int m_stn_idx;
};

