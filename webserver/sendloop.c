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
#include "reading.h"

#define SERVER_PORT 80

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

bool fill_buffer(int clsock, char* buffer, char until, int buffer_length, int* bytes_read) {
    int i = 0;
    while (i < buffer_length) {
        int rv = recv(clsock, &buffer[i], 1, 0);
        if (rv > 0) {
            i += rv;
        } else {
            perror("read");
            return true;
        }
        if (buffer[i - 1] == until) {
            break;
        }
    }
    *bytes_read = i;
    return false;
}

const char* method_not_allowed_response = "HTTP/1.0 405 Method Not Allowed\r\n\r\nError 405 (Method Not Allowed)\n";
const char* not_found_response = "HTTP/1.0 404 Not Found\r\n\r\nError 404 (Not Found)\n";

#define QUOTE(...) #__VA_ARGS__
const char* home_page = QUOTE(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Hamilton TCPlp Demo Web Page</title>

    <!-- Bootstrap CSS -->
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/css/bootstrap.min.css" integrity="sha384-WskhaSGFgHYWDcbwN70/dfYBj47jz9qbsMId/iRN3ewGhXQFZCSftd1LZCfmhktB" crossorigin="anonymous">
</head>

<body bgcolor="white">
    <nav class="navbar navbar-expand-lg navbar-light bg-light">
        <a class="navbar-brand">TCPlp/Hamilton Web Page</a>
        <ul class="nav nav-pills">
            <li class="nav-item">
                <a class="nav-link active" href=".">Home</a>
            </li>
            <li class="nav-item">
                <a class="nav-link" href="./network">Network</a>
            </li>
            <li class="nav-item">
                <a class="nav-link" href="./sensor">Sensors</a>
            </li>
        </ul>
    </nav>

    <div class="container">
        <div class="mt-3 mb-3 text-center">
            <h1>TCPlp/Hamilton Web Page</h1>
        </div>
        <div>
            <p>
                Welcome to the TCPlp/Hamilton Web Page!
                This page is hosted on a Hamilton networked sensor running the Thread network protocol.
                This web page was sent to your computer using TCP, over an IEEE 802.15.4 link.
                Every time you access this page, the LED on the sensor toggles (it turns off if it was on or on if it was off).
                Click the links at the top of this page to see more information.
            </p>
        </div>
    </div>

    <!-- Bootstrap Javascript -->
    <script src="https://code.jquery.com/jquery-3.3.1.slim.min.js" integrity="sha384-q8i/X+965DzO0rT7abK41JStQIAqVgRVzpbzo5smXKp4YfRvH+8abtTE1Pi6jizo" crossorigin="anonymous"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.3/umd/popper.min.js" integrity="sha384-ZMP7rVo3mIykV+2+9J3UJ46jBk0WLaUAdn689aCwoqbBJiSnjAK/l8WvCWPIPm49" crossorigin="anonymous"></script>
    <script src="https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/js/bootstrap.min.js" integrity="sha384-smHYKdLADwkXOn1EmN1qk/HfnUcbVRZyYmZ4qpPea6sjB/pTJ0euyQp0Mk8ck+5T" crossorigin="anonymous"></script>
</body>
);

const char* network_page_beginning = QUOTE(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Hamilton TCPlp Demo Web Page - Network</title>

    <!-- Bootstrap CSS -->
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/css/bootstrap.min.css" integrity="sha384-WskhaSGFgHYWDcbwN70/dfYBj47jz9qbsMId/iRN3ewGhXQFZCSftd1LZCfmhktB" crossorigin="anonymous">
</head>

<body bgcolor="white">
    <nav class="navbar navbar-expand-lg navbar-light bg-light">
        <a class="navbar-brand">TCPlp/Hamilton Web Page</a>
        <ul class="nav nav-pills">
            <li class="nav-item">
                <a class="nav-link" href="..">Home</a>
            </li>
            <li class="nav-item">
                <a class="nav-link active" href="../network">Network</a>
            </li>
            <li class="nav-item">
                <a class="nav-link" href="../sensor">Sensors</a>
            </li>
        </ul>
    </nav>

    <div class="container">
        <div class="mt-3 mb-3 text-center">
            <h1>Network Information and Statistics</h1>
        </div>
        <div>
            <p>
                This sensor runs the Thread network protocol, which is a full network stack, network layer and below.
                It provides multihop routing and 6LoWPAN over IEEE 802.15.4.
                TCP runs on top of Thread.
            </p>
        </div>
        <div>
            <table class="table">
                <thead>
                    <tr>
                        <th scope="col">Statistic</th>
                        <th scope="col">Value</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>TCP Segments Sent</td>
                        <td id="segssent">0</td>
                    </tr>
                    <tr>
                        <td>TCP Segments Received</td>
                        <td id="segsrcvd">0</td>
                    </tr>
                    <tr>
                        <td>TCP Timeouts</td>
                        <td id="timeouts">0</td>
                    </tr>
                    <tr>
                        <td>TCP Total Retransmissions</td>
                        <td id="trexmits">0</td>
                    </tr>
                    <tr>
                        <td>Link-Layer Frames Sent</td>
                        <td id="framsent">0</td>
                    </tr>
                    <tr>
                        <td>Link-Layer Frames Received</td>
                        <td id="framrcvd">0</td>
                    </tr>
                    <tr>
                        <td>Next Hop</td>
                        <td id="nexthopv">0</td>
                    </tr>
                    <tr>
                        <td>Cost to Border Router (Link Cost/Routing Cost)</td>
                        <td id="brtcosts">0</td>
                    </tr>
                    <tr>
                        <td>CPU Duty Cycle</td>
                        <td id="cpudutyc">0</td>
                    </tr>
                </tbody>
            </table>
        </div>
    </div>

    <script>
);
const char* network_page_middle_format = QUOTE(
        var data = {
            "segssent": "%u",
            "segsrcvd": "%u",
            "timeouts": "%u",
            "trexmits": "%u",
            "framsent": "%u",
            "framrcvd": "%u",
            "nexthopv": "%x",
            "brtcosts": "%u/%u",
            "cpudutyc": "%u.%02u%%"
        };
);
const char* network_page_end = QUOTE(
        for (var key in data) {
            if (data.hasOwnProperty(key)) {
                document.getElementById(key).innerHTML = data[key];
            }
        }
    </script>

    <!-- Bootstrap Javascript -->
    <script src="https://code.jquery.com/jquery-3.3.1.slim.min.js" integrity="sha384-q8i/X+965DzO0rT7abK41JStQIAqVgRVzpbzo5smXKp4YfRvH+8abtTE1Pi6jizo" crossorigin="anonymous"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.3/umd/popper.min.js" integrity="sha384-ZMP7rVo3mIykV+2+9J3UJ46jBk0WLaUAdn689aCwoqbBJiSnjAK/l8WvCWPIPm49" crossorigin="anonymous"></script>
    <script src="https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/js/bootstrap.min.js" integrity="sha384-smHYKdLADwkXOn1EmN1qk/HfnUcbVRZyYmZ4qpPea6sjB/pTJ0euyQp0Mk8ck+5T" crossorigin="anonymous"></script>
</body>
);

const char* sensor_page_beginning = QUOTE(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Hamilton TCPlp Demo Web Page - Network</title>

    <!-- Bootstrap CSS -->
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/css/bootstrap.min.css" integrity="sha384-WskhaSGFgHYWDcbwN70/dfYBj47jz9qbsMId/iRN3ewGhXQFZCSftd1LZCfmhktB" crossorigin="anonymous">
</head>

<body bgcolor="white">
    <nav class="navbar navbar-expand-lg navbar-light bg-light">
        <a class="navbar-brand">TCPlp/Hamilton Web Page</a>
        <ul class="nav nav-pills">
            <li class="nav-item">
                <a class="nav-link" href="..">Home</a>
            </li>
            <li class="nav-item">
                <a class="nav-link" href="../network">Network</a>
            </li>
            <li class="nav-item">
                <a class="nav-link active" href="../sensor">Sensors</a>
            </li>
        </ul>
    </nav>

    <div class="container">
        <div class="mt-3 mb-3 text-center">
            <h1>Current Sensor Readings</h1>
        </div>
        <div>
            <p>
                The Hamilton 3C platform has a variety of sensors, listed below.
                Sensor readings are accurate as of <span id="now">0</span>.
            </p>
        </div>
        <div>
            <table class="table">
                <thead>
                    <tr>
                        <th scope="col">Sensor</th>
                        <th scope="col">Value</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>Accelerometer</td>
                        <td id="acc">0</td>
                    </tr>
                    <tr>
                        <td>Magnetometer</td>
                        <td id="mag">0</td>
                    </tr>
                    <tr>
                        <td>Radiant Temperature (Die/Val)</td>
                        <td id="rdt">0</td>
                    </tr>
                    <tr>
                        <td>Temperature</td>
                        <td id="tmp">0</td>
                    </tr>
                    <tr>
                        <td>Humidity</td>
                        <td id="hum">0</td>
                    </tr>
                    <tr>
                        <td>Luminance</td>
                        <td id="lum">0</td>
                    </tr>
                    <tr>
                        <td>Button State</td>
                        <td id="but">0</td>
                    </tr>
                    <tr>
                        <td>Occupancy</td>
                        <td id="occ">0</td>
                    </tr>
                </tbody>
            </table>
        </div>
    </div>

    <script>
);
const char* sensor_page_middle_format = QUOTE(
        var data = {
            "acc": "<%d, %d, %d>",
            "mag": "<%d, %d, %d>",
            "rdt": "%d/%d",
            "tmp": "%u.%02u C",
            "hum": "%u.%02u%%",
            "lum": "%d",
            "but": "%u",
            "occ": "%u"
        };
);
const char* sensor_page_end = QUOTE(
        for (var key in data) {
            if (data.hasOwnProperty(key)) {
                document.getElementById(key).innerHTML = data[key];
            }
        }
        var now = new Date();
        document.getElementById("now").innerHTML = now.toString();
    </script>

    <!-- Bootstrap Javascript -->
    <script src="https://code.jquery.com/jquery-3.3.1.slim.min.js" integrity="sha384-q8i/X+965DzO0rT7abK41JStQIAqVgRVzpbzo5smXKp4YfRvH+8abtTE1Pi6jizo" crossorigin="anonymous"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.3/umd/popper.min.js" integrity="sha384-ZMP7rVo3mIykV+2+9J3UJ46jBk0WLaUAdn689aCwoqbBJiSnjAK/l8WvCWPIPm49" crossorigin="anonymous"></script>
    <script src="https://stackpath.bootstrapcdn.com/bootstrap/4.1.1/js/bootstrap.min.js" integrity="sha384-smHYKdLADwkXOn1EmN1qk/HfnUcbVRZyYmZ4qpPea6sjB/pTJ0euyQp0Mk8ck+5T" crossorigin="anonymous"></script>
</body>
);

#define BUFFER_SIZE 512
char buffer[BUFFER_SIZE];

#define RESOURCE_NAME_SIZE 32
char resource_name[RESOURCE_NAME_SIZE];

extern uint32_t transportPacketsSent;
extern uint32_t transportPacketsReceived;
extern uint32_t timeoutRexmitCnt;
extern uint32_t totalRexmitCnt;
extern uint32_t radioLinkTx;
extern uint32_t radioLinkRx;
extern uint16_t nextHopRloc;
extern uint8_t borderRouterLC;
extern uint8_t borderRouterRC;

void sample(ham7c_t *m);

void serve_page(int clsock) {
    /* Consume the header. */
    bool is_get_request = false;
    bool first_iteration = true;
    int bytes_read;
    do {
        if (fill_buffer(clsock, buffer, '\n', BUFFER_SIZE, &bytes_read)) {
            return;
        }
        if (buffer[bytes_read - 1] != '\n') {
            printf("HTTP header line too long\n");
        } else if (first_iteration) {
            int i;
            for (i = 0; buffer[i] != ' ' && i < bytes_read; i++);
            if (i < bytes_read) {
                buffer[i] = '\0';
                if (strcmp(buffer, "GET") == 0) {
                    is_get_request = true;
                    int start_index = i + 1;
                    for (i = start_index; buffer[i] != ' ' && i < bytes_read; i++);
                    if (i < bytes_read) {
                        buffer[i] = '\0';
                        strncpy(resource_name, &buffer[start_index], RESOURCE_NAME_SIZE);
                    }
                }
            }
            first_iteration = false;
        }
    } while (!(bytes_read == 1 || (bytes_read == 2 && buffer[0] == '\r')));

    if (!is_get_request) {
        write_string(clsock, method_not_allowed_response);
        return;
    }

    /* Write the response header */
    if (strcmp(resource_name, "/") == 0) {
        if (write_string(clsock, home_page)) {
            return;
        }
        LED_TOGGLE;
    } else if (strcmp(resource_name, "/network") == 0) {
        if (write_string(clsock, network_page_beginning)) {
            return;
        }
        unsigned cpudc = (unsigned) ((cpuOnTime * 10000) / (cpuOffTime + cpuOnTime));
        snprintf(buffer, BUFFER_SIZE, network_page_middle_format,
            (unsigned) transportPacketsSent,
            (unsigned) transportPacketsReceived,
            (unsigned) timeoutRexmitCnt,
            (unsigned) totalRexmitCnt,
            (unsigned) radioLinkTx,
            (unsigned) radioLinkRx,
            (unsigned) nextHopRloc,
            (unsigned) borderRouterLC,
            (unsigned) borderRouterRC,
            cpudc / 100,
            cpudc % 100
        );
        if (write_string(clsock, buffer)) {
            return;
        }
        if (write_string(clsock, network_page_end)) {
            return;
        }
    } else if (strcmp(resource_name, "/sensor") == 0) {
        if (write_string(clsock, sensor_page_beginning)) {
            return;
        }
        {
            ham7c_t reading;
            sample(&reading);
            snprintf(buffer, BUFFER_SIZE, sensor_page_middle_format,
                (int) reading.acc_x,
                (int) reading.acc_y,
                (int) reading.acc_z,
                (int) reading.mag_x,
                (int) reading.mag_y,
                (int) reading.mag_z,
                (int) reading.tmp_die,
                (int) reading.tmp_val,
                (unsigned) (reading.hdc_temp / 100),
                (unsigned) (reading.hdc_temp % 100),
                (unsigned) (reading.hdc_hum / 100),
                (unsigned) (reading.hdc_hum % 100),
                (int) reading.light_lux,
                (unsigned) reading.buttons,
                (unsigned) reading.occup
            );
        }
        if (write_string(clsock, buffer)) {
            return;
        }
        if (write_string(clsock, sensor_page_end)) {
            return;
        }
    } else {
        write_string(clsock, not_found_response);
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
                serve_page(clsock);
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
