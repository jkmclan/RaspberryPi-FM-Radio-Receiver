/*

A class which reads the Clock-Wise, 
Counter-Clock-Wise, and momentary 
switch state changes of an incremental 
rotary encoder using interrupts

Jeff McLane <jkmclane68@yahoo.com>

*/

#include <pthread.h>
#include <gpiod.h>
#include <time.h>

template <typename T>
class QueueThreadSafe;

class RotaryEncoderEvent {

public:

    typedef enum RotaryStates {
                       ROT_DECREMENT=-1,
                       ROT_NC=0,
                       ROT_INCREMENT=1,
                       ROT_SW_PUSHED=2
                      } RotaryStates_t;
 
    RotaryEncoderEvent(volatile int* do_exit,
                       QueueThreadSafe<RotaryEncoderEvent::RotaryStates>* tune_queue);

    ~RotaryEncoderEvent();

    bool init();

    pthread_t run();

protected:

private:

    //RotaryEncoderEvent() {};

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

    //static gpiod_ctxless_event_poll_cb poll_callback(unsigned int num_lines,
    static int poll_callback(unsigned int num_lines,
                                              struct gpiod_ctxless_event_poll_fd *fds,
                                              const struct timespec *timeout,
                                              void *data);

    //static gpiod_ctxless_event_handle_cb event_callback(int event_type,
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
                                       const struct timespec& rt_timestamp,
                                       const struct timespec& mono_timestamp);

    static QueueThreadSafe<RotaryEncoderEvent::RotaryStates>* s_tune_queue;

    static volatile int* s_do_exit;

    thread_data_t m_thread_data;
};

