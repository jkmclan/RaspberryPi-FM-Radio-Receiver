/*

A class which reads the Clock-Wise, 
Counter-Clock-Wise, and momentary 
switch state changes of an incremental 
rotary encoder using the sysfs interface 
on a Raspberry Pi.


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
    m_thread_data.offsets[0] = 23; // CLK
    m_thread_data.offsets[1] = 24; // DT
    m_thread_data.offsets[2] = 25; // SWITCH
    m_thread_data.num_lines = 2;
    m_thread_data.active_low = true;
    strcpy(m_thread_data.consumer, "JeffsRadio");
    m_thread_data.timeout = { 2, 0 };
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
                                const struct timespec& rt_timestamp,
                                const struct timespec& mono_timestamp)
{
    char evname[32];

    if (event_type == GPIOD_CTXLESS_EVENT_CB_RISING_EDGE)
        strcpy(evname, " RISING EDGE");
    else
        strcpy(evname, "FALLING EDGE");

    printf("event: %s offset: %u timestamp: [%8ld.%09ld]  RT: [%8ld.%09ld]  MONO: [%8ld.%09ld]\n",
           evname, offset, ts->tv_sec, ts->tv_nsec,
           rt_timestamp.tv_sec, rt_timestamp.tv_nsec,
           mono_timestamp.tv_sec, mono_timestamp.tv_nsec);
}

//gpiod_ctxless_event_poll_cb RotaryEncoderEvent::poll_callback(unsigned int num_lines,
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
/*
    close(thread_data->ctx.sigfd);

    return GPIOD_CTXLESS_EVENT_POLL_RET_STOP;
*/

}

//gpiod_ctxless_event_handle_cb RotaryEncoderEvent::event_callback(int event_type,
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

        if (thread_data->ctx.events_wanted && thread_data->ctx.events_done >= thread_data->ctx.events_wanted)
                return GPIOD_CTXLESS_EVENT_CB_RET_STOP;

        return GPIOD_CTXLESS_EVENT_CB_RET_OK;
}

void RotaryEncoderEvent::handle_event(thread_data_t* p_thread_data,
                                      int p_event_type,
                                      int p_offset,
                                      const struct timespec *p_timestamp)
{
    static struct timespec* a_ts = NULL;
    static struct timespec* b_ts = NULL;

    static bool state_sent = false;

    p_thread_data->ctx.events_done++;

    if (!p_thread_data->ctx.silent) {
        struct timespec rt_timestamp;
        if (clock_gettime(CLOCK_REALTIME, &rt_timestamp) == -1) {
            perror("clock_gettime(CLOCK_REALTIME error");
        }

        struct timespec mono_timestamp;
        if (clock_gettime(CLOCK_MONOTONIC, &mono_timestamp) == -1) {
            perror("clock_gettime(CLOCK_MONOTONIC error");
        }

        event_print_human_readable(p_offset, p_timestamp, p_event_type, rt_timestamp, mono_timestamp);
    }

    RotaryEncoderEvent::RotaryStates_t rs = ROT_NC;

    struct timespec now_ts;
    clock_gettime(CLOCK_MONOTONIC, &now_ts);

    if (p_offset == 23) {
        //
        // Debounce
        //

/*
        const int num_pins = 2;
        unsigned int offsets[num_pins] = { 23, 24 };
        int* values = (int*) malloc(sizeof(*values) * num_pins);
        int rv = gpiod_ctxless_get_value_multiple(thread_data->gpio_chip_name,
                                                      offsets,
                                                      values,
                                                      num_pins,
                                                      thread_data->active_low,
                                                      thread_data->consumer);
                                                      //thread_data->flags);
        if (rv = -1) perror("gpiod_ctxless_get_value_multiple - errno");
        int a = values[0];
        int b = values[1];
        free(values);
*/
        unsigned int offset = 24;
        int value;
/*
        value = gpiod_ctxless_get_value(p_thread_data->gpio_chip_name,
                                                offset,
                                                p_thread_data->active_low,
                                                p_thread_data->consumer);
                                                //p_thread_data->flags);
        if (value = -1) perror("gpiod_ctxless_get_value_ext - errno");
*/

/*
        const char *device = p_thread_data->gpio_chip_name;
        const unsigned int *offsets = &offset;
        int *values = &value;
        unsigned int num_lines = 1;
        bool active_low = p_thread_data->active_low;
        const char *consumer = p_thread_data->consumer;
        int flags = 0;

        struct gpiod_line_bulk bulk;
        struct gpiod_chip *chip;
        struct gpiod_line *line;
        unsigned int i;
        int rv, req_flags;

        if (!num_lines || num_lines > GPIOD_LINE_BULK_MAX_LINES) {
                errno = EINVAL;
                return;
        }

        chip = gpiod_chip_open_lookup(device);
        if (!chip)
                return;

        gpiod_line_bulk_init(&bulk);

        for (i = 0; i < num_lines; i++) {
                line = gpiod_chip_get_line(chip, offsets[i]);
                if (!line) {
                        gpiod_chip_close(chip);
                        return;
                }

                gpiod_line_bulk_add(&bulk, line);
        }

        req_flags = GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW;
        rv = gpiod_line_request_bulk_input_flags(&bulk, consumer, req_flags);
        if (rv < 0) {
                gpiod_chip_close(chip);
                return;
        }

        memset(values, 0, sizeof(*values) * num_lines);
        rv = gpiod_line_get_value_bulk(&bulk, values);

        gpiod_chip_close(chip);
*/
///////////////////
/*
        int a = 1;
        int b = value;
        printf("a | b : %d | %d\n", a, b);

        if (a != a0) {              // A changed
            a0 = a;
            if (b != c0) {
                c0 = b;
                if (a == b) {
                    rs = ROT_INCREMENT;
                    a0 = 0;
                    c0 = 0;
                } else {
                    rs = ROT_DECREMENT;
                    a0 = 0;
                    c0 = 0;
                }
            }
        }
*/
//////////////////////
        if (!p_thread_data->ctx.silent) printf("%d - Here 1\n", p_offset);
        struct timespec diff_ts;
        if (a_ts != NULL) {
            diff_ts.tv_sec = now_ts.tv_sec - a_ts->tv_sec;
            diff_ts.tv_nsec = now_ts.tv_nsec - a_ts->tv_nsec;
        } else if (b_ts != NULL) {
            diff_ts.tv_sec = now_ts.tv_sec - b_ts->tv_sec;
            diff_ts.tv_nsec = now_ts.tv_nsec - b_ts->tv_nsec;
        }
        if (diff_ts.tv_nsec < 0) {
            diff_ts.tv_nsec += 1000000000; // nsec/sec
            diff_ts.tv_sec--;
        }

        if (!p_thread_data->ctx.silent) printf("%d - Here 2 - diff_ts = [%8ld.%09ld]\n", p_offset, diff_ts.tv_sec, diff_ts.tv_nsec);
        //if ( (diff_ts.tv_sec > 0) || (diff_ts.tv_nsec > 117725912) ) {
        if ( (diff_ts.tv_sec > 0) || (diff_ts.tv_nsec > 250000000) ) {
            if (!p_thread_data->ctx.silent) printf("%d - Here 3\n", p_offset);

            if (a_ts != NULL) {
                if (!p_thread_data->ctx.silent) printf("%d - Here 4\n", p_offset);
                delete a_ts;
                a_ts = NULL;
            }

            if (b_ts != NULL) {
                if (!p_thread_data->ctx.silent) printf("%d - Here 5\n", p_offset);
                delete b_ts;
                b_ts = NULL;
            }
            state_sent = false;
        }

        if ((b_ts == NULL) && (a_ts == NULL)) {
            if (!p_thread_data->ctx.silent) printf("%d - Here 6\n", p_offset);
            a_ts = new timespec();
            clock_gettime(CLOCK_MONOTONIC, a_ts);
        }
    } else if (p_offset == 24) {
        if (!p_thread_data->ctx.silent) printf("%d - Here 1\n", p_offset);

        struct timespec diff_ts;
        if (a_ts != NULL) {
            diff_ts.tv_sec = now_ts.tv_sec - a_ts->tv_sec;
            diff_ts.tv_nsec = now_ts.tv_nsec - a_ts->tv_nsec;
        } else if (b_ts != NULL) {
            diff_ts.tv_sec = now_ts.tv_sec - b_ts->tv_sec;
            diff_ts.tv_nsec = now_ts.tv_nsec - b_ts->tv_nsec;
        }
        if (diff_ts.tv_nsec < 0) {
            diff_ts.tv_nsec += 1000000000; // nsec/sec
            diff_ts.tv_sec--;
        }

        if (!p_thread_data->ctx.silent) printf("%d - Here 2 - diff_ts = [%8ld.%09ld]\n", p_offset, diff_ts.tv_sec, diff_ts.tv_nsec);
        if ( (diff_ts.tv_sec > 0) || (diff_ts.tv_nsec > 250000000) ) {
            if (!p_thread_data->ctx.silent) printf("%d - Here 3\n", p_offset);

            if (a_ts != NULL) {
                if (!p_thread_data->ctx.silent) printf("%d - Here 4\n", p_offset);
                delete a_ts;
                a_ts = NULL;
            }

            if (b_ts != NULL) {
                if (!p_thread_data->ctx.silent) printf("%d - Here 5\n", p_offset);
                delete b_ts;
                b_ts = NULL;
            }
            state_sent = false;
        }


        if ((a_ts == NULL) && (b_ts == NULL)) {
            if (!p_thread_data->ctx.silent) printf("%d - Here 6\n", p_offset);
            b_ts = new timespec();
            clock_gettime(CLOCK_MONOTONIC, b_ts);
        }
//////////////////////
    } else if (p_offset == 25) {
        rs = ROT_SW_PUSHED;
    }

    if (!p_thread_data->ctx.silent) printf("%d - Here 7\n", p_offset);

    if (!state_sent && (a_ts != NULL) && (b_ts == NULL)) {
        if (!p_thread_data->ctx.silent) printf("%d - Here 8\n", p_offset);
        rs = ROT_INCREMENT;
        state_sent = true;
    }
    else if (!state_sent && (b_ts != NULL) && (a_ts == NULL)) {
        if (!p_thread_data->ctx.silent) printf("%d - Here 9\n", p_offset);
        rs = ROT_DECREMENT;
        state_sent = true;
    } else {
        if (!p_thread_data->ctx.silent) printf("Here 10 - still debouncing\n");
    }

    //
    // Notify the controller
    //

    if (rs != ROT_NC) {
        printf("RotaryEncoderEvent::handle_event: pushed %d\n", rs);
        s_tune_queue->push(rs);
    }
}

