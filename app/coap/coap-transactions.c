/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
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
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      CoAP module for reliable transport
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

/**
 * \addtogroup coap
 * @{
 */

#include "coap-transactions.h"
#include "coap-observe.h"
#include "coap-timer.h"
#include "lib/memb.h"
#include "lib/list.h"
#include <stdlib.h>

/* Added for random numbers. */
#include "random.h"

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "coap"
#define LOG_LEVEL  LOG_LEVEL_COAP
#ifdef COCOA
#define PRINTF LOG_DBG
#endif

/*---------------------------------------------------------------------------*/
MEMB(transactions_memb, coap_transaction_t, COAP_MAX_OPEN_TRANSACTIONS);
LIST(transactions_list);

/*---------------------------------------------------------------------------*/
extern uint32_t timeoutRexmitCnt;
extern uint32_t totalRexmitCnt;
static void
coap_retransmit_transaction(coap_timer_t *nt)
{
  coap_transaction_t *t = coap_timer_get_user_data(nt);
  if(t == NULL) {
    LOG_DBG("No retransmission data in coap_timer!\n");
    return;
  }
  ++(t->retrans_counter);
  LOG_DBG("Retransmitting %u (%u)\n", t->mid, t->retrans_counter);
  timeoutRexmitCnt++;
  totalRexmitCnt++;
  coap_send_transaction(t);
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
coap_transaction_t *
coap_new_transaction(uint16_t mid, const coap_endpoint_t *endpoint)
{
#ifdef COCOA
    //AUGUST
    // if(countTransactionsForAddress(addr, transactions_list) >= NSTART){
    if(countTransactionsForAddress(&endpoint->ipaddr, transactions_list) >= NSTART){
        PRINTF("NSTART limit reached!\n");
        return NULL;
    }
#endif
  coap_transaction_t *t = memb_alloc(&transactions_memb);

  if(t) {
    t->mid = mid;
    t->retrans_counter = 0;

    /* save client address */
    coap_endpoint_copy(&t->endpoint, endpoint);

    list_add(transactions_list, t); /* list itself makes sure same element is not added twice */
  }

  return t;
}
/*---------------------------------------------------------------------------*/
void
coap_send_transaction(coap_transaction_t *t)
{
  LOG_DBG("Sending transaction %u\n", t->mid);

  if(COAP_TYPE_CON ==
     ((COAP_HEADER_TYPE_MASK & t->message[0]) >> COAP_HEADER_TYPE_POSITION)) {
    if(t->retrans_counter <= COAP_MAX_RETRANSMIT) {
      /* not timed out yet */
      coap_sendto(&t->endpoint, t->message, t->message_len);
      LOG_DBG("Keeping transaction %u\n", t->mid);

      if(t->retrans_counter == 0) {
        coap_timer_set_callback(&t->retrans_timer, coap_retransmit_transaction);
        coap_timer_set_user_data(&t->retrans_timer, t);
#ifdef COCOA
        // clock_time_t storedRto = coap_check_rtt_estimation(&t->addr, transactions_list);
        clock_time_t storedRto = coap_check_rtt_estimation(&t->endpoint.ipaddr, transactions_list);
        //t->retrans_timer.timer.interval = storedRto+ random_rand()% (storedRto >> 1);
        //t->retrans_interval = storedRto + rand() % (storedRto >> 1);
        t->retrans_interval = storedRto + random_uint32() % (storedRto >> 1);
        t->start_rto = storedRto;
        //t->timestamp = clock_time();
        t->timestamp = coap_timer_uptime();
#else
        t->retrans_interval =
          //COAP_RESPONSE_TIMEOUT_TICKS + (rand() %
          COAP_RESPONSE_TIMEOUT_TICKS + (random_uint32() %
                                         COAP_RESPONSE_TIMEOUT_BACKOFF_MASK);
#endif
        LOG_DBG("Initial interval %lu msec\n",
                (unsigned long)t->retrans_interval);
      } else {
#ifdef COCOA
        // t->retrans_timer.timer.interval = coap_check_rto_state(t->retrans_timer.timer.interval, t->start_rto);
        t->retrans_interval = coap_check_rto_state(t->retrans_interval, t->start_rto);
        //t->retrans_timer.timer.interval = (t->retrans_timer.timer.interval > MAXRTO_VALUE) ? MAXRTO_VALUE : t->retrans_timer.timer.interval;
        PRINTF("Increased (%u) interval %f\n", t->retrans_counter,
                            //    (float)t->retrans_timer.timer.interval / CLOCK_SECOND);
                               (float)t->retrans_interval / CLOCK_SECOND);
#else
        t->retrans_interval <<= 1;  /* double */
        LOG_DBG("Doubled (%u) interval %lu s\n", t->retrans_counter,
                (unsigned long)(t->retrans_interval / 1000));
#endif
      }

      /* interval updated above */
      coap_timer_set(&t->retrans_timer, t->retrans_interval);
    } else {
      /* timed out */
      LOG_DBG("Timeout\n");
      coap_resource_response_handler_t callback = t->callback;
      void *callback_data = t->callback_data;

      /* handle observers */
      coap_remove_observer_by_client(&t->endpoint);

      coap_clear_transaction(t);

      if(callback) {
        callback(callback_data, NULL);
      }
    }
  } else {
    coap_sendto(&t->endpoint, t->message, t->message_len);
    coap_clear_transaction(t);
  }
}
/*---------------------------------------------------------------------------*/
void
coap_clear_transaction(coap_transaction_t *t)
{
  if(t) {
    LOG_DBG("Freeing transaction %u: %p\n", t->mid, t);
#ifdef COCOA
    if(t->retrans_counter <= COAP_MAX_RETRANSMIT){
        //August: Check if this was a CONFIRMABLE that actually provides RTT measurements
        //if(COAP_TYPE_CON==((COAP_HEADER_TYPE_MASK & t->packet[0])>>COAP_HEADER_TYPE_POSITION)){
        if(COAP_TYPE_CON==((COAP_HEADER_TYPE_MASK & t->message[0])>>COAP_HEADER_TYPE_POSITION)){
            //AUGUST: before clearing transaction store RTT info
            clock_time_t rtt = clock_time() - t->timestamp;
            // coap_update_rtt_estimation(&t->addr, rtt, t->retrans_counter);
            coap_update_rtt_estimation(&t->endpoint.ipaddr, rtt, t->retrans_counter);
        }
    }
#endif
    coap_timer_stop(&t->retrans_timer);
    list_remove(transactions_list, t);
    memb_free(&transactions_memb, t);
  }
}
/*---------------------------------------------------------------------------*/
coap_transaction_t *
coap_get_transaction_by_mid(uint16_t mid)
{
  coap_transaction_t *t = NULL;

  for(t = (coap_transaction_t *)list_head(transactions_list); t; t = t->next) {
    if(t->mid == mid) {
      LOG_DBG("Found transaction for MID %u: %p\n", t->mid, t);
      return t;
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/** @} */
