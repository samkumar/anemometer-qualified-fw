#include <stdio.h>
#include <rtt_stdio.h>
#include "ot.h"
#include <openthread/openthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sched.h"
#include <cib.h>
#include <cond.h>
#include <mutex.h>

#include "anemometer.h"

measure_set_t readings[READING_BUF_SIZE];
cib_t readings_cib = CIB_INIT(READING_BUF_SIZE);
mutex_t readings_mutex = MUTEX_INIT;
cond_t readings_cond = COND_INIT;

char chunk_buf[SEND_CHUNK_SIZE];

void send_measurement_loop(void) {
    int sock;
    int rv;
    for (;;) {
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

        for (;;) {
            size_t partway_size_copied = 0;
            int index;
            /*
             * This loop does the following:
             * 1) Wait until the next measurement is ready
             * 2) Send it over TCP
             */
            mutex_lock(&readings_mutex);
            while (cib_avail(&readings_cib) < READING_SEND_LIMIT) {
                cond_wait(&readings_cond, &readings_mutex);
            }
            mutex_unlock(&readings_mutex);

            /* Repeatedly send chunks until the readings buffer is empty. */
            bool hit_empty;
            do {
                /* First, fill the chunk buffer with data. */
                mutex_lock(&readings_mutex);
                size_t chunk_buf_index = 0;
                do {
                    index = cib_peek(&readings_cib);
                    assert(index != -1);
                    size_t chunk_buf_left = sizeof(chunk_buf) - chunk_buf_index;
                    size_t measure_set_left = sizeof(measure_set_t) - partway_size_copied;
                    if (chunk_buf_left >= measure_set_left) {
                        memcpy(&chunk_buf[chunk_buf_index], &((char*) &readings[index])[partway_size_copied], measure_set_left);
                        chunk_buf_index += measure_set_left;
                        cib_get(&readings_cib);
                        partway_size_copied = 0;
                    } else {
                        memcpy(&chunk_buf[chunk_buf_index], &((char*) &readings[index])[partway_size_copied], chunk_buf_left);
                        partway_size_copied += chunk_buf_left;
                        chunk_buf_index = sizeof(chunk_buf);
                    }
                } while (!(hit_empty = (cib_avail(&readings_cib) == 0)) && chunk_buf_index != sizeof(chunk_buf));
                mutex_unlock(&readings_mutex);

                /* Second, send out the data (with the lock released). */
                rv = send(sock, chunk_buf, chunk_buf_index, 0);
                if (rv == -1) {
                    perror("send");
                    goto retry;
                }
            } while (!hit_empty);
        }

    retry:
        close(sock);
        /* Wait three seconds before trying to connect again. */
        xtimer_usleep(3000000u);
    }
}

void* sendloop(void* arg) {
    (void) arg;
    send_measurement_loop();
    return NULL;
}

static kernel_pid_t sendloop_pid = 0;
//static char sendloop_stack[1392];
static char sendloop_stack[3000] __attribute__((aligned(4)));
kernel_pid_t start_sendloop(void)
{
    if (sendloop_pid != 0) {
        return sendloop_pid;
    }
    otIp6AddUnsecurePort(openthread_get_instance(), SENDER_PORT);
    sendloop_pid = thread_create(sendloop_stack, sizeof(sendloop_stack),
                                 THREAD_PRIORITY_MAIN - 1,
                                 THREAD_CREATE_STACKTEST, sendloop, NULL,
                                 "sendloop");
    return sendloop_pid;
}
