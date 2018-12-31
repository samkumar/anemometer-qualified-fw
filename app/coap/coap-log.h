/*
 * Copyright (c) 2017, RISE SICS.
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
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *         Log support for CoAP
 * \author
 *         Niclas Finne <niclas.finne@ri.se>
 *         Joakim Eriksson <joakim.eriksson@ri.se>
 */

/**
 * \addtogroup coap
 * @{
 */

#ifndef COAP_LOG_H_
#define COAP_LOG_H_

/*#include "contiki.h"*/

#ifdef COAP_LOG_CONF_PATH
#include COAP_LOG_CONF_PATH
#else /* COAP_LOG_CONF_PATH */
/*#include "sys/log.h"*/

/* Chosen/implemented by samkumar. */
#include <stdio.h>
#include <inttypes.h>
#define LOG_LEVEL_COAP 0

/* Copied from Contiki's sys/log-conf.h (by samkumar). */
#define LOG_WITH_LOC 0
#define LOG_WITH_MODULE_PREFIX 0
#define LOG_OUTPUT_PREFIX(level, levelstr, module) LOG_OUTPUT("[%-4s: %-10s] ", levelstr, module)
#define LOG_OUTPUT(...) printf(__VA_ARGS__)

/* Copied from Contiki's sys/log.h (by samkumar). */

/* The different log levels available */
#define LOG_LEVEL_NONE         0 /* No log */
#define LOG_LEVEL_ERR          1 /* Errors */
#define LOG_LEVEL_WARN         2 /* Warnings */
#define LOG_LEVEL_INFO         3 /* Basic info */
#define LOG_LEVEL_DBG          4 /* Detailled debug */

/* Main log function */

#define LOG(newline, level, levelstr, ...) do {  \
                            if(level <= (LOG_LEVEL)) { \
                              if(newline) { \
                                if(LOG_WITH_MODULE_PREFIX) { \
                                  LOG_OUTPUT_PREFIX(level, levelstr, LOG_MODULE); \
                                } \
                                if(LOG_WITH_LOC) { \
                                  LOG_OUTPUT("[%s: %d] ", __FILE__, __LINE__); \
                                } \
                              } \
                              LOG_OUTPUT(__VA_ARGS__); \
                            } \
                          } while (0)

/* For Cooja annotations */
#define LOG_ANNOTATE(...) do {  \
                            if(LOG_WITH_ANNOTATE) { \
                              LOG_OUTPUT(__VA_ARGS__); \
                            } \
                        } while (0)

/* Link-layer address */
#define LOG_LLADDR(level, lladdr) do {  \
                            if(level <= (LOG_LEVEL)) { \
                              if(LOG_WITH_COMPACT_ADDR) { \
                                log_lladdr_compact(lladdr); \
                              } else { \
                                log_lladdr(lladdr); \
                              } \
                            } \
                        } while (0)

/* IPv6 address */
#define LOG_6ADDR(level, ipaddr) do {  \
                           if(level <= (LOG_LEVEL)) { \
                             if(LOG_WITH_COMPACT_ADDR) { \
                               log_6addr_compact(ipaddr); \
                             } else { \
                               log_6addr(ipaddr); \
                             } \
                           } \
                         } while (0)

/* More compact versions of LOG macros */
#define LOG_PRINT(...)         LOG(1, 0, "PRI", __VA_ARGS__)
#define LOG_ERR(...)           LOG(1, LOG_LEVEL_ERR, "ERR", __VA_ARGS__)
#define LOG_WARN(...)          LOG(1, LOG_LEVEL_WARN, "WARN", __VA_ARGS__)
#define LOG_INFO(...)          LOG(1, LOG_LEVEL_INFO, "INFO", __VA_ARGS__)
#define LOG_DBG(...)           LOG(1, LOG_LEVEL_DBG, "DBG", __VA_ARGS__)

#define LOG_PRINT_(...)         LOG(0, 0, "PRI", __VA_ARGS__)
#define LOG_ERR_(...)           LOG(0, LOG_LEVEL_ERR, "ERR", __VA_ARGS__)
#define LOG_WARN_(...)          LOG(0, LOG_LEVEL_WARN, "WARN", __VA_ARGS__)
#define LOG_INFO_(...)          LOG(0, LOG_LEVEL_INFO, "INFO", __VA_ARGS__)
#define LOG_DBG_(...)           LOG(0, LOG_LEVEL_DBG, "DBG", __VA_ARGS__)
#endif /* COAP_LOG_CONF_PATH */

#include "coap-endpoint.h"

/* CoAP endpoint */
#define LOG_COAP_EP(level, endpoint) do {                   \
    if(level <= (LOG_LEVEL)) {                              \
      coap_endpoint_log(endpoint);                          \
    }                                                       \
  } while (0)

#define LOG_ERR_COAP_EP(endpoint)  LOG_COAP_EP(LOG_LEVEL_ERR, endpoint)
#define LOG_WARN_COAP_EP(endpoint) LOG_COAP_EP(LOG_LEVEL_WARN, endpoint)
#define LOG_INFO_COAP_EP(endpoint) LOG_COAP_EP(LOG_LEVEL_INFO, endpoint)
#define LOG_DBG_COAP_EP(endpoint)  LOG_COAP_EP(LOG_LEVEL_DBG, endpoint)

/* CoAP strings */
#define LOG_COAP_STRING(level, text, len) do {              \
    if(level <= (LOG_LEVEL)) {                              \
      coap_log_string(text, len);                           \
    }                                                       \
  } while (0)

#define LOG_ERR_COAP_STRING(text, len)  LOG_COAP_STRING(LOG_LEVEL_ERR, text, len)
#define LOG_WARN_COAP_STRING(text, len) LOG_COAP_STRING(LOG_LEVEL_WARN, text, len)
#define LOG_INFO_COAP_STRING(text, len) LOG_COAP_STRING(LOG_LEVEL_INFO, text, len)
#define LOG_DBG_COAP_STRING(text, len)  LOG_COAP_STRING(LOG_LEVEL_DBG, text, len)

/**
 * \brief Logs a CoAP string that has a length but might not be 0-terminated.
 * \param text The CoAP string
 * \param len  The number of characters in the CoAP string
 */
void coap_log_string(const char *text, size_t len);

#endif /* COAP_LOG_H_ */
/** @} */
