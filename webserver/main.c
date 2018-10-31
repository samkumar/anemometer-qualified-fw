#include <stdio.h>
#include <rtt_stdio.h>
#include "shell.h"
#include "thread.h"
#include "xtimer.h"
#include <string.h>
#include "msg.h"
#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/udp.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netreg.h"
#include "phydat.h"
#include "saul_reg.h"
#include <periph/gpio.h>
#include "mutex.h"
#include "cond.h"


/* I need these variables to make OpenThread compile. */
uint16_t myRloc = 0;

#ifdef CPU_DUTYCYCLE_MONITOR
volatile uint64_t cpuOnTime = 0;
volatile uint64_t cpuOffTime = 0;
volatile uint32_t contextSwitchCnt = 0;
volatile uint32_t preemptCnt = 0;
#endif
#ifdef RADIO_DUTYCYCLE_MONITOR
uint64_t radioOnTime = 0;
uint64_t radioOffTime = 0;
#endif

uint32_t packetSuccessCnt = 0;
uint32_t packetFailCnt = 0;
uint32_t packetBusyChannelCnt = 0;
uint32_t broadcastCnt = 0;
uint32_t queueOverflowCnt = 0;

uint16_t nextHopRloc = 0;
uint8_t borderRouterLC = 0;
uint8_t borderRouterRC = 0;
uint32_t routeChangeCnt = 0;
uint32_t borderRouteChangeCnt = 0;

uint32_t totalIpv6MsgCnt = 0;
uint32_t Ipv6TxSuccCnt = 0;
uint32_t Ipv6TxFailCnt = 0;
uint32_t Ipv6RxSuccCnt = 0;
uint32_t Ipv6RxFailCnt = 0;

uint32_t pollMsgCnt = 0;
uint32_t mleMsgCnt = 0;

uint32_t mleRouterMsgCnt = 0;
uint32_t addrMsgCnt = 0;
uint32_t netdataMsgCnt = 0;

uint32_t meshcopMsgCnt = 0;
uint32_t tmfMsgCnt = 0;

uint32_t totalSerialMsgCnt = 0;

uint32_t radioLinkTx = 0;
uint32_t radioLinkRx = 0;
uint32_t radioPollTx = 0;
uint32_t pollTimeoutCnt = 0;
uint32_t transportPacketsSent = 0;
uint32_t transportPacketsReceived = 0;
uint32_t timeoutRexmitCnt = 0;
uint32_t totalRexmitCnt = 0;
uint32_t sendBatch = 0;
uint32_t sendBatchSliding = 0;
uint64_t radioListenTime = 0;


uint64_t start_time = 0;
void start_listening(void) {
    assert(start_time == 0);
    start_time = xtimer_now_usec64();
}
void end_listening(void) {
    uint64_t now = xtimer_now_usec64();
    radioListenTime += (now - start_time);
    start_time = 0;
}

// 1 second, defined in us
#define INTERVAL (1000000U)
#define NETWORK_RTT_US 1000000
#define COUNT_TX (-4)
#define A_LONG_TIME 5000000U
#define MAIN_QUEUE_SIZE     (8)
#define TMP_I2C_ADDRESS 0x40
#define DIRECT_DATA_ADDRESS 0x65
#define OPENTHREAD_INIT_TIME 5000000ul

void start_sendloop(void);

//Anemometer v2
#define L7_TYPE 9
int main(void)
{
    xtimer_usleep(OPENTHREAD_INIT_TIME);
    start_sendloop();

    return 0;
}
