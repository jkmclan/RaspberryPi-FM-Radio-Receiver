/*

A class which reads the Clock-Wise,
Counter-Clock-Wise, and momentary
switch state changes of an incremental
rotary encoder using the gpiod library
for interrupt-based queue events.

Jeff McLane <jkmclane68@yahoo.com>

*/

#include <pthread.h>
#include <gpiod.h>
#include <time.h>

template <typename T>
class QueueThreadSafe;

#define PIN_CLK 23
#define PIN_DT  24
#define PIN_SW  25

class RotaryEncoderEvent {

public:

    typedef enum RotaryStates {
                       ROT_DECREMENT=-1,
                       ROT_NC=0,
                       ROT_INCREMENT=1,
                       ROT_SW_PUSHED=5
                      } RotaryStates_t;
 
    RotaryEncoderEvent(volatile int* do_exit,
                       QueueThreadSafe<RotaryEncoderEvent::RotaryStates>* tune_queue);

    ~RotaryEncoderEvent();

    bool init();

    pthread_t run();

protected:

private:

    typedef struct mon_ctx {

        //
        // User data provided for the GPIOD callbacks
        //
        unsigned int events_wanted;
        unsigned int events_done;
        bool silent;
        int sigfd;

    } mon_ctx_t;

    typedef struct thread_data {

        //
        // User data provided for the thread function
        //
        char            gpio_chip_name[128];
        int             event_type;
        unsigned int    offsets[GPIOD_LINE_BULK_MAX_LINES];
        unsigned int    num_lines;
        bool            active_low;
        char            consumer[128];
        struct timespec timeout;        
        //gpiod_ctxless_event_poll_cb poll_callback;
        //gpiod_ctxless_event_handle_cb event_callback;
        mon_ctx_t       ctx;
        int             flags;    

    } thread_data_t;

    static void* start(void* data);

    static int poll_callback(unsigned int num_lines,
                             struct gpiod_ctxless_event_poll_fd *fds,
                             const struct timespec *timeout,
                             void *data);

    static int event_callback(int event_type,
                              unsigned int line_offset,
                              const struct timespec *timestamp,
                              void *data);

    static void handle_event(thread_data_t* thread_data,
                             int event_type,
                             int line_offset,
                             const struct timespec *timestamp);

    int make_signalfd();

    static void event_print_human_readable(unsigned int offset,
                                           const struct timespec *ts,
                                           int event_type,
                                           const struct timespec& now_ts);

    static timespec diff_timespec(int& offset,
                                  timespec& now,
                                  timespec* first_ts,
                                  thread_data_t* thread_data);

    static timespec* check_debouncing_timeout(int& offset,
                                              timespec* first_active_ts,
                                              timespec& diff_ts,
                                              thread_data_t* thread_data);

    static QueueThreadSafe<RotaryEncoderEvent::RotaryStates>* s_tune_queue;

    static volatile int* s_do_exit;

    thread_data_t m_thread_data;
};

