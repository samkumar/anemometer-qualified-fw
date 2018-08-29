/*
 * Copyright (c) 2016, SICS, Swedish ICT AB.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * \file
 *         CoAP timer driver implementation based on Contiki etimers
 * \author
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 */

/**
 * \addtogroup coap-timer
 * @{
 *
 * \defgroup coap-timer-default CoAP timer for Contiki-NG
 * @{
 *
 * This is an implementation of CoAP timer for Contiki-NG.
 */

#include "coap-timer.h"
// #include "sys/clock.h"
// #include "sys/etimer.h"
// #include "sys/process.h"

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "coap-timer"
#define LOG_LEVEL  LOG_LEVEL_NONE

/* RIOT dependencies (added by Sam). */
#include "thread.h"
#include "xtimer.h"
#include "ot.h"

// PROCESS(coap_timer_process, "coap timer process");

// static uint64_t current_time;
// static struct etimer timer;
static xtimer_t timer;
static msg_t timer_msg;
kernel_pid_t coap_timer_thread_pid;
/*---------------------------------------------------------------------------*/
static void
update_timer(void)
{
  uint64_t remaining;
  remaining = coap_timer_time_to_next_expiration();
  LOG_DBG("remaining %lu msec\n", (unsigned long)remaining);
  if(remaining == 0) {
    /* Run as soon as possible */
    // process_poll(&coap_timer_process);
    msg_send(&timer_msg, coap_timer_thread_pid);
  } else {
    // remaining *= CLOCK_SECOND;
    // remaining /= 1000;
    // if(remaining > CLOCK_SECOND * 60) {
    //   /* Make sure the CoAP timer clock is updated at least once per minute */
    //   remaining = CLOCK_SECOND * 60;
    // } else if(remaining < 1) {
    //   /* Wait minimum one system clock tick */
    //   remaining = 1;
    // }
    // etimer_set(&timer, (clock_time_t)remaining);
    if (remaining > 60 * 1000) {
        /* Make sure the CoAP timer clock is updated at least once per minute */
        remaining = 60 * 1000;
    } else if (remaining < 1) {
        /* Wait minimum 1 millisecond */
        remaining = 1;
    }
    xtimer_set_msg64(&timer, 1000 * remaining, &timer_msg, coap_timer_thread_pid);
  }
}
/*---------------------------------------------------------------------------*/
// PROCESS_THREAD(coap_timer_process, ev, data)
// {
//   PROCESS_BEGIN();
//
//   etimer_set(&timer, CLOCK_SECOND);
//   while(1) {
//     PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER ||
//                              ev == PROCESS_EVENT_POLL);
//
//     if(coap_timer_run()) {
//       /* Needs to run again */
//       process_poll(&coap_timer_process);
//     } else {
//       update_timer();
//     }
//   }
//   PROCESS_END();
// }
void start_polling_fast(void);
void* coap_timer_process(void* arg) {
    (void) arg;

    msg_t msg;
    msg_t msg_queue[1];

    msg_init_queue(msg_queue, 1);

    for (;;) {
        msg_receive(&msg);

        openthread_lock_coarse_mutex();
        switch (msg.type) {
        case 0:
            if (coap_timer_run()) {
                /* Needs to run again. */
                msg_send_to_self(&msg);
            } else {
                update_timer();
            }
            break;
        case 1:
            start_polling_fast();
            break;
        default:
            assert(false);
            for (;;) {
                printf("Unknown message type %d\n", msg.type);
            }
            break;
        }
        openthread_unlock_coarse_mutex();
    }

    return NULL;
}
/*---------------------------------------------------------------------------*/
static uint64_t
uptime(void)
{
  // static clock_time_t last;
  // clock_time_t now;
  // uint64_t diff;
  //
  // now = clock_time();
  // diff = (clock_time_t)(now - last);
  // if(diff > 0) {
  //   current_time += (diff * 1000) / CLOCK_SECOND;
  //   last = now;
  // }
  // return current_time;
  return xtimer_now_usec64() / 1000;
}
/*---------------------------------------------------------------------------*/
static char timer_stack[1024];
static void
init(void)
{
  // process_start(&coap_timer_process, NULL);
  timer_msg.type = 0;
  coap_timer_thread_pid = thread_create(timer_stack, sizeof(timer_stack),
                             13, THREAD_CREATE_STACKTEST,
                             coap_timer_process, NULL, "coap_timer");
}
/*---------------------------------------------------------------------------*/
static void
update(void)
{
  // process_poll(&coap_timer_process);
  msg_send(&timer_msg, coap_timer_thread_pid);
}
/*---------------------------------------------------------------------------*/
const coap_timer_driver_t coap_timer_default_driver = {
  .init = init,
  .uptime = uptime,
  .update = update,
};
/*---------------------------------------------------------------------------*/
/** @} */
/** @} */
