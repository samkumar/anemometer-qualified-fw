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

#include "board.h"

#define SERVER_PORT 5000

bool write_string(int clsock, const char* string) {
    int length = strlen(string);
    int i = 0;
    while (i < length) {
        int rv = send(clsock, &string[i], length - i, 0);
        if (rv > 0) {
            i += rv;
        } else {
            perror("write");
            return true;
        }
    }
    return false;
}

bool fill_newline_buffer(int clsock, char* buffer, int buffer_length, int* bytes_read) {
    int i = 0;
    while (i < buffer_length) {
        int rv = recv(clsock, &buffer[i], 1, 0);
        if (rv > 0) {
            i += rv;
        } else {
            perror("read");
            return true;
        }
        if (buffer[i - 1] == '\n') {
            break;
        }
    }
    *bytes_read = i;
    return false;
}

#define BUFFER_SIZE 256
char buffer[BUFFER_SIZE];

void run_shell(int clsock) {
    for (;;) {
        if (write_string(clsock, "TCPlp shell> ")) {
            return;
        }

        int bytes_read;
        if (fill_newline_buffer(clsock, buffer, BUFFER_SIZE, &bytes_read)) {
            return;
        }
        if (bytes_read == 0 || bytes_read == 1) {
            continue;
        }
        if (buffer[bytes_read - 1] != '\n') {
            if (write_string(clsock, "Input too long\n")) {
                return;
            }
            continue;
        }

        /* Eliminate whitespace. */
        int start;
        int end;
        for (start = 0; buffer[start] == ' '; start++);
        for (end = start; buffer[end] != ' ' && buffer[end] != '\n'; end++);
        if (start == end) {
            continue;
        }
        buffer[end] = '\0';
        const char* command = &buffer[start];

        bool wrote = false;
        if (strcmp(command, "ledon") == 0) {
            LED_ON;
        } else if (strcmp(command, "ledoff") == 0) {
            LED_OFF;
        } else if (strcmp(command, "ledtoggle") == 0) {
            LED_TOGGLE;
        } else {
            if (write_string(clsock, "Invalid command ")) {
                return;
            }
            if (write_string(clsock, command)) {
                return;
            }
            wrote = true;
        }

        if (wrote && write_string(clsock, "\n")) {
            return;
        }
    }
}

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
            while ((clsock = accept(sock, (struct sockaddr*) &peer, &peer_len)) != -1) {
                run_shell(clsock);
                close(clsock);
            }
        }

    retry:
        if (sock != -1) {
            close(sock);
        }
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
