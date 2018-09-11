#include <stdio.h>
#include <rtt_stdio.h>
#include "ot.h"
#include <openthread/openthread.h>
#ifdef USE_TCP
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#ifdef USE_COAP
#include "coap/coap.h"
#include "coap/coap-engine.h"
#include "coap/coap-callback-api.h"
#endif

#include "sched.h"
#include <cond.h>
#include <mutex.h>

#include "anemometer.h"

measure_set_t readings[READING_BUF_SIZE];
mutex_t readings_mutex = MUTEX_INIT;
cond_t readings_cond = COND_INIT;

/* The actual buffer. */
int read_next = 0;
int write_next = 0;
int buffer_size = 0;
int buffer_put(void) {
    if (buffer_size == READING_BUF_SIZE) {
        return -1;
    }
    int result = write_next;
    write_next++;
    buffer_size++;
    if (write_next == READING_BUF_SIZE) {
        write_next = 0;
    }
    return result;
}
int buffer_peek(void) {
    if (buffer_size == 0) {
        return -1;
    }
    return read_next;
}
int buffer_get(void) {
    if (buffer_size == 0) {
        return -1;
    }
    int result = read_next;
    read_next++;
    buffer_size--;
    if (read_next == READING_BUF_SIZE) {
        read_next = 0;
    }
    return result;
}
int buffer_avail(void) {
    return buffer_size;
}

char chunk_buf[SEND_CHUNK_SIZE+1];

#ifdef USE_COAP
extern uint32_t sendBatch;

mutex_t coap_blocking_mutex = MUTEX_INIT;
cond_t coap_blocking_cond = COND_INIT;
bool coap_request_done = false;

//static uint32_t coap_token = 0;
static uint16_t coap_seqno = 0;

xtimer_t start_polling_timer;
msg_t start_polling_message;
volatile bool polling_message_outstanding = false;
extern kernel_pid_t coap_timer_thread_pid;
void start_polling_fast(void) {
    if (polling_message_outstanding) {
        otThreadSetMaxPollInterval(openthread_get_instance(), 100);
        polling_message_outstanding = false;
    }
}
void start_polling_callback(void* arg) {
    /* Schedule a thread to set polling interval to 100ms. */
    (void) arg;
    if (polling_message_outstanding) {
        return;
    }
    polling_message_outstanding = true;
    msg_t start_polling_message;
    start_polling_message.type = 1;
    if (msg_send(&start_polling_message, coap_timer_thread_pid) != 1) {
        assert(false);
        for (;;) {
            printf("couldn't send message\n");
        }
    }
}
void schedule_fast_polling(void) {
    start_polling_timer.callback = start_polling_callback;
    start_polling_timer.arg = NULL;
    xtimer_set(&start_polling_timer, 100000ul);
}
void stop_polling_fast(void) {
    xtimer_remove(&start_polling_timer);
    /*
     * Now, timer is not running, so callback either executed or will not
     * for the remainder of this function. The acting thread, that is executing
     * start_polling_fast, will first acquire the coarse lock.
     */
    openthread_lock_coarse_mutex();
    polling_message_outstanding = false;
    otThreadSetMaxPollInterval(openthread_get_instance(), 0);
    openthread_unlock_coarse_mutex();
}


/*
 * This function is called for each block received, and then again with a
 * NULL response.
 */
void coap_blocking_request_callback(coap_request_state_t* state) {
    if (state->response == NULL) {
        mutex_lock(&coap_blocking_mutex);
        coap_request_done = true;
        cond_signal(&coap_blocking_cond);
        mutex_unlock(&coap_blocking_mutex);
    }
}
void coap_blocking_request(coap_endpoint_t* server_endpoint, coap_message_t* request) {
    coap_request_state_t request_state;
    memset(&request_state, 0x00, sizeof(request_state));
    assert(!coap_request_done);
    openthread_lock_coarse_mutex();
    coap_send_request(&request_state, server_endpoint, request, coap_blocking_request_callback);
    openthread_unlock_coarse_mutex();
    mutex_lock(&coap_blocking_mutex);
    while (!coap_request_done) {
        cond_wait(&coap_blocking_cond, &coap_blocking_mutex);
    }
    // Reset for the next request
    coap_request_done = false;
    mutex_unlock(&coap_blocking_mutex);
}
void coap_nonconfirmable_request_callback(coap_request_state_t* state) {
    (void) state;
    // Should not be reached!
    assert(false);
}
void coap_nonconfirmable_request(coap_endpoint_t* server_endpoint, coap_message_t* request) {
    coap_request_state_t request_state;
    memset(&request_state, 0x00, sizeof(request_state));
    assert(!coap_request_done);
    openthread_lock_coarse_mutex();
    coap_send_request(&request_state, server_endpoint, request, coap_nonconfirmable_request_callback);
    openthread_unlock_coarse_mutex();
    xtimer_usleep(500000ul);
}
#endif

void send_measurement_loop(void) {
    for (;;) {
#ifdef USE_TCP
        int sock;
        int rv;
        {
            sock = socket(AF_INET6, SOCK_STREAM, 0);
            if (sock == -1) {
                perror("socket");
                goto retry;
            }

            struct sockaddr_in6 receiver;
            receiver.sin6_family = AF_INET6;
            receiver.sin6_port = htons(RECEIVER_PORT);

            rv = inet_pton(AF_INET6, RECEIVER_IP, &receiver.sin6_addr);
            if (rv == -1) {
                perror("invalid address family in inet_pton");
                goto retry;
            } else if (rv == 0) {
                perror("invalid ip address in inet_pton");
                goto retry;
            }

            struct sockaddr_in6 local;
            local.sin6_family = AF_INET6;
            local.sin6_addr = in6addr_any;
            local.sin6_port = htons(SENDER_PORT);

            rv = bind(sock, (struct sockaddr*) &local, sizeof(struct sockaddr_in6));
            if (rv == -1) {
                perror("bind");
                goto retry;
            }

            rv = connect(sock, (struct sockaddr*) &receiver, sizeof(struct sockaddr_in6));
            if (rv == -1) {
                perror("connect");
                goto retry;
            }
        }
#endif
#ifdef USE_COAP
        static coap_endpoint_t endpoint;
        #define ENDPOINT_STR "coap://[" RECEIVER_IP "]"
        if (coap_endpoint_parse(ENDPOINT_STR, sizeof(ENDPOINT_STR)-1, &endpoint) == 0) {
            printf("Could not parse endpoint\n");
            assert(false);
        }
#endif

        for (;;) {
            size_t partway_size_copied = 0;
            int index;
            /*
             * This loop does the following:
             * 1) Wait until the next measurement is ready
             * 2) Send it over TCP
             */
            mutex_lock(&readings_mutex);
            while (buffer_avail() < READING_SEND_LIMIT) {
                cond_wait(&readings_cond, &readings_mutex);
            }
            mutex_unlock(&readings_mutex);

            /* Repeatedly send chunks until the readings buffer is empty. */
#ifdef USE_COAP
#ifndef COAP_NOCONFIRM
            schedule_fast_polling();
#endif
#ifdef SEND_IN_BATCHES
            uint32_t block_num = 0;
            //coap_token++;
#endif
#endif
            bool hit_empty;
            do {
                /* First, fill the chunk buffer with data. */
                mutex_lock(&readings_mutex);
                size_t chunk_buf_index = 0;
                do {
                    index = buffer_peek();
                    assert(index != -1);
                    size_t chunk_buf_left = SEND_CHUNK_SIZE - chunk_buf_index;
                    size_t measure_set_left = sizeof(measure_set_t) - partway_size_copied;
                    if (chunk_buf_left >= measure_set_left) {
                        memcpy(&chunk_buf[chunk_buf_index], &((char*) &readings[index])[partway_size_copied], measure_set_left);
                        chunk_buf_index += measure_set_left;
                        buffer_get();
                        partway_size_copied = 0;
                    } else {
                        memcpy(&chunk_buf[chunk_buf_index], &((char*) &readings[index])[partway_size_copied], chunk_buf_left);
                        partway_size_copied += chunk_buf_left;
                        chunk_buf_index = SEND_CHUNK_SIZE;
                    }
                } while (!(hit_empty = (buffer_avail() == 0)) && chunk_buf_index != SEND_CHUNK_SIZE);
                mutex_unlock(&readings_mutex);

                /* Second, send out the data (with the lock released). */
#ifdef USE_TCP
                rv = send(sock, chunk_buf, chunk_buf_index, 0);
                if (rv == -1) {
                    perror("send");
                    goto retry;
                }
#endif
#ifdef USE_COAP
#ifdef SEND_IN_BATCHES
                {
                    static coap_message_t request;
#ifdef COAP_NOCONFIRM
                    coap_init_message(&request, COAP_TYPE_NON, COAP_PUT, coap_seqno++);
#else
                    coap_init_message(&request, COAP_TYPE_CON, COAP_POST, coap_seqno++);
#endif
                    //coap_set_token(&request, (const uint8_t*) &coap_token, sizeof(coap_token));
#ifdef USE_BLOCKWISE_TRANSFER
                    coap_set_header_uri_path(&request, "/anemometer");
                    coap_set_header_block1(&request, block_num++, hit_empty ? 0 : 1, SEND_CHUNK_SIZE);
                    coap_set_payload(&request, chunk_buf, chunk_buf_index);
#else
                    coap_set_header_uri_path(&request, "/anechunked");
                    chunk_buf[chunk_buf_index] = block_num++;
                    coap_set_payload(&request, chunk_buf, chunk_buf_index+1);
#endif

#ifdef COAP_NOCONFIRM
                    coap_nonconfirmable_request(&endpoint, &request);
#else
                    coap_blocking_request(&endpoint, &request);
#endif
                }
#else
                {
                    //coap_token++;

                    static coap_message_t request;
#ifdef COAP_NOCONFIRM
                    coap_init_message(&request, COAP_TYPE_NON, COAP_PUT, coap_seqno++);
#else
                    coap_init_message(&request, COAP_TYPE_CON, COAP_POST, coap_seqno++);
#endif
                    coap_set_header_uri_path(&request, "/anemometer");
                    //coap_set_token(&request, (const uint8_t*) &coap_token, sizeof(coap_token));
                    coap_set_payload(&request, chunk_buf, chunk_buf_index);

                    sendBatch++;
#ifdef COAP_NOCONFIRM
                    coap_nonconfirmable_request(&endpoint, &request);
#else
                    coap_blocking_request(&endpoint, &request);
#endif
                }
#endif
#endif
            } while (!hit_empty);
#ifdef USE_COAP
#ifndef COAP_NOCONFIRM
            stop_polling_fast();
#endif
#endif
        }

#ifdef USE_TCP
    retry:
        close(sock);
        /* Wait three seconds before trying to connect again. */
        xtimer_usleep(3000000u);
#endif
    }
}

void* sendloop(void* arg) {
    (void) arg;
    send_measurement_loop();
    return NULL;
}

static kernel_pid_t sendloop_pid = 0;
//static char sendloop_stack[1392];
#ifdef USE_TCP
static char sendloop_stack[3000] __attribute__((aligned(4)));
#endif
#ifdef USE_COAP
static char sendloop_stack[2000] __attribute__((aligned(4)));
#endif
kernel_pid_t start_sendloop(void)
{
    if (sendloop_pid != 0) {
        return sendloop_pid;
    }
#ifdef USE_TCP
    otIp6AddUnsecurePort(openthread_get_instance(), SENDER_PORT);
#endif
#ifdef USE_COAP
    otIp6AddUnsecurePort(openthread_get_instance(), COAP_DEFAULT_PORT);
    coap_engine_init();
#endif
    sendloop_pid = thread_create(sendloop_stack, sizeof(sendloop_stack),
                                 THREAD_PRIORITY_MAIN - 1,
                                 THREAD_CREATE_STACKTEST, sendloop, NULL,
                                 "sendloop");
    return sendloop_pid;
}
