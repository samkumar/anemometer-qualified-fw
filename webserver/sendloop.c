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

#define SERVER_PORT 80

const char* webpage = "HTTP/1.1 200 OK\n\nHello, world!\n";
void send_measurement_loop(void) {
    for (;;) {
        int sock;
        int rv;
        {
            sock = socket(AF_INET6, SOCK_STREAM, 0);
            if (sock == -1) {
                perror("socket");
                goto retry;
            }

            struct sockaddr_in6 local;
            local.sin6_family = AF_INET6;
            local.sin6_addr = in6addr_any;
            local.sin6_port = htons(SERVER_PORT);

            rv = bind(sock, (struct sockaddr*) &local, sizeof(struct sockaddr_in6));
            if (rv == -1) {
                perror("bind");
                goto retry;
            }

            if (listen(sock, 1) != 0) {
                perror("listen");
                goto retry;
            }

            struct sockaddr_in6 peer;
            socklen_t peer_len;

            int clsock;
            int length = strlen(webpage);
            while ((clsock = accept(sock, (struct sockaddr*) &peer, &peer_len)) != -1) {
                int i = 0;
                while (i < length) {
                    rv = send(clsock, &webpage[i], length - i, 0);
                    if (rv > 0) {
                        i += rv;
                    } else {
                        perror("send");
                        break;
                    }
                }
                close(clsock);
            }
        }

    retry:
        if (sock != -1) {
            close(sock);
        }
    }
}

void* sendloop(void* arg) {
    (void) arg;
    send_measurement_loop();
    return NULL;
}

static kernel_pid_t sendloop_pid = 0;
static char sendloop_stack[2000] __attribute__((aligned(4)));
kernel_pid_t start_sendloop(void)
{
    if (sendloop_pid != 0) {
        return sendloop_pid;
    }
    otIp6AddUnsecurePort(openthread_get_instance(), SERVER_PORT);
    sendloop_pid = thread_create(sendloop_stack, sizeof(sendloop_stack),
                                 THREAD_PRIORITY_MAIN - 1,
                                 THREAD_CREATE_STACKTEST, sendloop, NULL,
                                 "sendloop");
    return sendloop_pid;
}
