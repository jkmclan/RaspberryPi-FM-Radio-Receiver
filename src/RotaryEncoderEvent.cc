/*

A class which reads the Clock-Wise, 
Counter-Clock-Wise, and momentary 
switch state changes of an incremental 
rotary encoder using the gpiod library
for interrupt-based queue events.


Jeff McLane <jkmclane68@yahoo.com>

*/

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <sys/signalfd.h>

#include "QueueThreadSafe.hh"
#include "RotaryEncoderEvent.hh"

//
// Initialize Static members
//

QueueThreadSafe<RotaryEncoderEvent::RotaryStates>* RotaryEncoderEvent::s_tune_queue = NULL;
volatile int* RotaryEncoderEvent::s_do_exit = NULL;

//
// Class Declaration
//
RotaryEncoderEvent::RotaryEncoderEvent(volatile int* do_exit,
                                       QueueThreadSafe<RotaryEncoderEvent::RotaryStates>* tune_queue)
{
    RotaryEncoderEvent::s_do_exit = do_exit;
    RotaryEncoderEvent::s_tune_queue = tune_queue;
}

RotaryEncoderEvent::~RotaryEncoderEvent() {
}

bool RotaryEncoderEvent::init() {

    memset(&m_thread_data, 0, sizeof(&m_thread_data));

    strcpy(m_thread_data.gpio_chip_name, "gpiochip0");
    m_thread_data.event_type = GPIOD_CTXLESS_EVENT_RISING_EDGE;
    m_thread_data.offsets[0] = PIN_CLK; // CLK
    m_thread_data.offsets[1] = PIN_DT; // DT
    m_thread_data.offsets[2] = PIN_SW; // SWITCH
    m_thread_data.num_lines = 3;
    m_thread_data.active_low = true;
    strcpy(m_thread_data.consumer, "JeffsRadio");
    m_thread_data.timeout = { 5, 0 };
    //m_thread_data.poll_callback = poll_callback;
    //m_thread_data.event_callback = event_callback;
    m_thread_data.flags = GPIOD_CTXLESS_FLAG_BIAS_PULL_UP;

    m_thread_data.ctx.events_wanted = 0;
    m_thread_data.ctx.events_done;
    m_thread_data.ctx.silent = true;
    m_thread_data.ctx.sigfd = -1;
    //m_thread_data.ctx.sigfd = make_signalfd();

    return true;
}

pthread_t RotaryEncoderEvent::run() {

    pthread_t thread_id;
    int stat = pthread_create(&thread_id, NULL, RotaryEncoderEvent::start, (void *)(&m_thread_data));

    if (stat != 0) {
        char buf[128];
        sprintf(buf, "RotaryEncoderEvent::run: pthread_create error (%d)", stat);
        perror(buf);
        return -1; 
    } else {
        return thread_id;
    }
}

void* RotaryEncoderEvent::start(void* data) {

    thread_data_t* thread_data = (thread_data_t*) data;

    static int rv = gpiod_ctxless_event_monitor_multiple_ext(
                                thread_data->gpio_chip_name,
                                thread_data->event_type,
                                thread_data->offsets,
                                thread_data->num_lines,
                                thread_data->active_low,
                                thread_data->consumer,
                                &(thread_data->timeout),
                                RotaryEncoderEvent::poll_callback,
                                RotaryEncoderEvent::event_callback,
                                (void*) thread_data,
                                thread_data->flags);
    if (rv != 0) {
        perror("RotaryEncoderEvent::start: gpiod_ctxless_event_monitor_multiple_ext terminated with error");
    } else {
        perror("RotaryEncoderEvent::start: gpiod_ctxless_event_monitor_multiple_ext terminated successfully");
    }

    return (void*) &rv;
}

int RotaryEncoderEvent::make_signalfd(void)
{
        sigset_t sigmask;
        int sigfd, rv = 0;

        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGTERM);
        sigaddset(&sigmask, SIGINT);

        rv = sigprocmask(SIG_BLOCK, &sigmask, NULL);
        if (rv < 0) {
            perror("error masking signals");
            return -1;
        }

        sigfd = signalfd(-1, &sigmask, 0);
        if (sigfd < 0) {
            perror("error creating signalfd");
            return -1;
        }

        return sigfd;
}

void RotaryEncoderEvent::event_print_human_readable(unsigned int offset,
                                                    const struct timespec *ts,
                                                    int event_type,
                                                    const struct timespec& now_ts)
{
    char evname[32];

    if (event_type == GPIOD_CTXLESS_EVENT_CB_RISING_EDGE)
        strcpy(evname, " RISING EDGE");
    else
        strcpy(evname, "FALLING EDGE");

    fprintf(stderr, "event: %s offset: %u Event Generated Time (MONOTONIC) : [%8ld.%09ld]  : Event Handled Time (MONOTONIC) : [%8ld.%09ld]\n",
           evname, offset, ts->tv_sec, ts->tv_nsec,
           now_ts.tv_sec, now_ts.tv_nsec);
}

int RotaryEncoderEvent::poll_callback(unsigned int num_lines,
                                      struct gpiod_ctxless_event_poll_fd *fds,
                                      const struct timespec *timeout,
                                      void *data)
{
    struct pollfd pfds[GPIOD_LINE_BULK_MAX_LINES + 1];
    thread_data_t* thread_data = (thread_data_t*) data;
    int cnt, ts, rv;
    unsigned int i;

    for (i = 0; i < num_lines; i++) {
        pfds[i].fd = fds[i].fd;
        pfds[i].events = POLLIN | POLLPRI;
    }

    //pfds[i].fd = thread_data->ctx.sigfd;
    //pfds[i].events = POLLIN | POLLPRI;

    ts = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000;

    cnt = poll(pfds, num_lines, ts);
    if (cnt < 0)
        return GPIOD_CTXLESS_EVENT_POLL_RET_ERR;
    else if (cnt == 0)
        if (*s_do_exit == 1)
            return GPIOD_CTXLESS_EVENT_POLL_RET_STOP;
        else
            return GPIOD_CTXLESS_EVENT_POLL_RET_TIMEOUT;

    rv = cnt;
    for (i = 0; i < num_lines; i++) {
        if (pfds[i].revents) {
            fds[i].event = true;
            if (!--cnt)
                return rv;
        }
    }

    return GPIOD_CTXLESS_EVENT_POLL_RET_TIMEOUT;

    /*
     * If we're here, then there's a signal pending. No need to read it,
     * we know we should quit now.
    */
    //close(thread_data->ctx.sigfd);
    //return GPIOD_CTXLESS_EVENT_POLL_RET_STOP;

}

int RotaryEncoderEvent::event_callback(int event_type,
                                       unsigned int line_offset,
                                       const struct timespec *timestamp,
                                       void *data)
{
        thread_data_t* thread_data = (thread_data_t*) data;

        switch (event_type) {
        case GPIOD_CTXLESS_EVENT_CB_RISING_EDGE:
        case GPIOD_CTXLESS_EVENT_CB_FALLING_EDGE:
                handle_event(thread_data, event_type, line_offset, timestamp);
                break;
        default:
                /*
                 * REVISIT: This happening would indicate a problem in the
                 * library.
                 */

                return GPIOD_CTXLESS_EVENT_CB_RET_OK;
        }

        if (thread_data->ctx.events_wanted &&
            thread_data->ctx.events_done >= thread_data->ctx.events_wanted)
                return GPIOD_CTXLESS_EVENT_CB_RET_STOP;

        return GPIOD_CTXLESS_EVENT_CB_RET_OK;
}

void RotaryEncoderEvent::handle_event(thread_data_t* p_thread_data,
                                      int p_event_type,
                                      int p_offset,
                                      const struct timespec *p_timestamp)
{
    static struct timespec* first_active_ts = NULL;

    static RotaryEncoderEvent::RotaryStates_t rs = ROT_NC;

    p_thread_data->ctx.events_done++;

    // Get current timestamp
    struct timespec now_ts;
    clock_gettime(CLOCK_MONOTONIC, &now_ts);

    // Logging - Event Timing
    if (!p_thread_data->ctx.silent) {
        event_print_human_readable(p_offset, p_timestamp, p_event_type, now_ts);
    }

    //
    // Identify which offset (aka pin number) is active
    //

    switch (p_offset) {

    case PIN_CLK: {
        if (!p_thread_data->ctx.silent) fprintf(stderr, "%d - Here 1\n", p_offset);

        // Get the period from now to the first active pin detection
        timespec diff_ts = diff_timespec(p_offset, now_ts, first_active_ts, p_thread_data);

        // Determine whether this event is a bounce or a new active rotation
        first_active_ts = check_debouncing_timeout(p_offset, first_active_ts, diff_ts, p_thread_data);

        if (first_active_ts == NULL) {

            //
            // Detected clockwise rotation
            //

            if (!p_thread_data->ctx.silent) fprintf(stderr, "%d - Here 5\n", p_offset);
            first_active_ts = new timespec();
            clock_gettime(CLOCK_MONOTONIC, first_active_ts);
 
            rs = ROT_INCREMENT;
        }

        break;
    }
    case PIN_DT: {
        if (!p_thread_data->ctx.silent) fprintf(stderr, "%d - Here 1\n", p_offset);

        // Get the period from now to the first active pin detection
        timespec diff_ts = diff_timespec(p_offset, now_ts, first_active_ts, p_thread_data);

        // Determine whether this event is a bounce or a new active rotation
        first_active_ts = check_debouncing_timeout(p_offset, first_active_ts, diff_ts, p_thread_data);

        if (first_active_ts == NULL) {

            //
            // Detected counter-clockwise rotation
            //

            if (!p_thread_data->ctx.silent) fprintf(stderr, "%d - Here 5\n", p_offset);
            first_active_ts = new timespec();
            clock_gettime(CLOCK_MONOTONIC, first_active_ts);

            rs = ROT_DECREMENT;
        }

        break;
    }
    case PIN_SW: {
        if (!p_thread_data->ctx.silent) fprintf(stderr, "%d - Here 1\n", p_offset);

        // Get the period from now to the first active pin detection
        timespec diff_ts = diff_timespec(p_offset, now_ts, first_active_ts, p_thread_data);

        // Determine whether this event is a bounce or a new active rotation
        first_active_ts = check_debouncing_timeout(p_offset, first_active_ts, diff_ts, p_thread_data);

        if (first_active_ts == NULL) {

            //
            // Detected momentary switch activation
            //

            if (!p_thread_data->ctx.silent) fprintf(stderr, "%d - Here 5\n", p_offset);
            first_active_ts = new timespec();
            clock_gettime(CLOCK_MONOTONIC, first_active_ts);
 
            rs = ROT_SW_PUSHED;
        }

        break;
    }
    default: {
        fprintf(stderr, "RotaryEncoderEvent.handle_event(): Unexpected offset: %d\n", p_offset);
    }
    }

    //
    // Test for valid rotation vs. debouncing
    //

    if (rs == ROT_NC) {
        //
        // Debouncing
        //

        if (!p_thread_data->ctx.silent) fprintf(stderr, "Here 6 - Debouncing\n");
    } else {
        //
        // Notify the controller
        //

        fprintf(stderr, "RotaryEncoderEvent::handle_event: pushed %d\n", rs);
        s_tune_queue->push(rs);
        rs = ROT_NC;
    }
}

timespec RotaryEncoderEvent::diff_timespec(int& offset,
                                           timespec& now_ts,
                                           timespec* first_ts,
                                           thread_data_t* thread_data) {

    struct timespec diff_ts = {.tv_sec = 0, .tv_nsec = 0};

    if (first_ts != NULL) {
            diff_ts.tv_sec = now_ts.tv_sec - first_ts->tv_sec;
            diff_ts.tv_nsec = now_ts.tv_nsec - first_ts->tv_nsec;
    }

    if (diff_ts.tv_nsec < 0) {
        // Adjust the difference
        diff_ts.tv_nsec += 1000000000; // nsec/sec
        diff_ts.tv_sec--;
    }

    if (!thread_data->ctx.silent)
        fprintf(stderr, "%d - Here 2 - diff_ts = [%8ld.%09ld]\n", offset, diff_ts.tv_sec, diff_ts.tv_nsec);

    return diff_ts;
}

timespec* RotaryEncoderEvent::check_debouncing_timeout(int& offset,
                                                       timespec* first_active_ts,
                                                       timespec& diff_ts,
                                                       thread_data_t* thread_data) {

    if ( (diff_ts.tv_sec > 0) || (diff_ts.tv_nsec > 250000000) ) {
        if (!thread_data->ctx.silent) fprintf(stderr, "%d - Here 3\n", offset);

        if (first_active_ts != NULL) {
            if (!thread_data->ctx.silent) fprintf(stderr, "%d - Here 4\n", offset);
            delete first_active_ts;
            first_active_ts = NULL;
        }
    }

    return first_active_ts;
}
