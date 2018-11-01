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
#include "reading.h"

#define ENABLE_DEBUG (0)
#include "debug.h"


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

#define TYPE_FIELD 8

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

//Anemometer v2
#define L7_TYPE 9

saul_reg_t *sensor_radtemp_t = NULL;
saul_reg_t *sensor_temp_t    = NULL;
saul_reg_t *sensor_hum_t     = NULL;
saul_reg_t *sensor_mag_t     = NULL;
saul_reg_t *sensor_accel_t   = NULL;
saul_reg_t *sensor_light_t   = NULL;
saul_reg_t *sensor_occup_t   = NULL;
saul_reg_t *sensor_button_t  = NULL;

//Uptime is mandatory (for AES security)
#define FLAG_ACC    (1<<0)
#define FLAG_MAG    (1<<1)
#define FLAG_TMP    (1<<2)
#define FLAG_HDC    (1<<3)
#define FLAG_LUX     (1<<4)
#define FLAG_BUTTONS  (1<<5)
#define FLAG_OCCUP    (1<<6)

#if defined(MODEL_3C)
#define PROVIDED_FLAGS (0x3F)
#elif defined(MODEL_7C)
#define PROVIDED_FLAGS (0x7F)
#else
#error "No model defined"
#endif

void critical_error(void) {
    DEBUG("CRITICAL ERROR, REBOOT\n");
    NVIC_SystemReset();
    return;
}

void sensor_config(void) {
    sensor_radtemp_t = saul_reg_find_type(SAUL_SENSE_RADTEMP);
    if (sensor_radtemp_t == NULL) {
        DEBUG("[ERROR] Failed to init RADTEMP sensor\n");
        critical_error();
    } else {
        DEBUG("TEMP sensor OK\n");
    }

    sensor_hum_t     = saul_reg_find_type(SAUL_SENSE_HUM);
    if (sensor_hum_t == NULL) {
        DEBUG("[ERROR] Failed to init HUM sensor\n");
        critical_error();
    } else {
        DEBUG("HUM sensor OK\n");
    }

    sensor_temp_t    = saul_reg_find_type(SAUL_SENSE_TEMP);
    if (sensor_temp_t == NULL) {
		DEBUG("[ERROR] Failed to init TEMP sensor\n");
		critical_error();
	} else {
		DEBUG("TEMP sensor OK\n");
	}

    sensor_mag_t     = saul_reg_find_type(SAUL_SENSE_MAG);
    if (sensor_mag_t == NULL) {
		DEBUG("[ERROR] Failed to init MAGNETIC sensor\n");
		critical_error();
	} else {
		DEBUG("MAGNETIC sensor OK\n");
	}

    sensor_accel_t   = saul_reg_find_type(SAUL_SENSE_ACCEL);
    if (sensor_accel_t == NULL) {
		DEBUG("[ERROR] Failed to init ACCEL sensor\n");
		critical_error();
	} else {
		DEBUG("ACCEL sensor OK\n");
	}

    sensor_light_t   = saul_reg_find_type(SAUL_SENSE_LIGHT);
	if (sensor_light_t == NULL) {
		DEBUG("[ERROR] Failed to init LIGHT sensor\n");
		critical_error();
	} else {
		DEBUG("LIGHT sensor OK\n");
	}

    sensor_occup_t   = saul_reg_find_type(SAUL_SENSE_OCCUP);
	if (sensor_occup_t == NULL) {
		DEBUG("[ERROR] Failed to init OCCUP sensor\n");
		critical_error();
	} else {
		DEBUG("OCCUP sensor OK\n");
	}

    sensor_button_t  = saul_reg_find_type(SAUL_SENSE_COUNT);
    if (sensor_button_t == NULL) {
        DEBUG("[ERROR] Failed to init BUTTON sensor\n");
        critical_error();
    } else {
        DEBUG("BUTTON sensor OK\n");
    }
}

/* ToDo: Sampling sequence arrangement or thread/interrupt based sensing may be better */
void sample(ham7c_t *m) {
    phydat_t output; /* Sensor output data (maximum 3-dimension)*/
	   int dim;         /* Demension of sensor output */

    /* Occupancy 1-dim */
    #if ((PROVIDED_FLAGS & FLAG_OCCUP) != 0)
    dim = saul_reg_read(sensor_occup_t, &output);
    if (dim > 0) {
        m->occup = output.val[0];
        // printf("\nDev: %s\tType: %s\n", sensor_occup_t->name,
        //         saul_class_to_str(sensor_occup_t->driver->type));
        // phydat_dump(&output, dim);
    } else {
        DEBUG("[ERROR] Failed to read Occupancy\n");
    }
    #endif

    /* Push button events 1-dim */
    dim = saul_reg_read(sensor_button_t, &output);
    if (dim > 0) {
        m->buttons = output.val[0];
        // printf("\nDev: %s\tType: %s\n", sensor_button_t->name,
        //         saul_class_to_str(sensor_button_t->driver->type));
        //phydat_dump(&output, dim);
    } else {
        DEBUG("[ERROR] Failed to read button events\n");
    }

    /* Illumination 1-dim */
	dim = saul_reg_read(sensor_light_t, &output);
	if (dim > 0) {
		m->light_lux = output.val[0];
		// printf("\nDev: %s\tType: %s\n", sensor_light_t->name,
		// 				saul_class_to_str(sensor_light_t->driver->type));
		//phydat_dump(&output, dim);
	} else {
		DEBUG("[ERROR] Failed to read Illumination\n");
	}

    /* Magnetic field 3-dim */
    dim = saul_reg_read(sensor_mag_t, &output);
    if (dim > 0) {
        m->mag_x = output.val[0]; m->mag_y = output.val[1]; m->mag_z = output.val[2];
        // printf("\nDev: %s\tType: %s\n", sensor_mag_t->name,
        //         saul_class_to_str(sensor_mag_t->driver->type));
        //phydat_dump(&output, dim);
    } else {
        DEBUG("[ERROR] Failed to read magnetic field\n");
    }

    /* Acceleration 3-dim */
    dim = saul_reg_read(sensor_accel_t, &output);
    if (dim > 0) {
        m->acc_x = output.val[0]; m->acc_y = output.val[1]; m->acc_z = output.val[2];
        // printf("\nDev: %s\tType: %s\n", sensor_accel_t->name,
        //         saul_class_to_str(sensor_accel_t->driver->type));
        //phydat_dump(&output, dim);
    } else {
        printf("[ERROR] Failed to read Acceleration\n");
    }

    /* Radient temperature 1-dim */
    dim = saul_reg_read(sensor_radtemp_t, &output); /* 500ms */
    if (dim > 0) {
        m->tmp_die = output.val[1];
        m->tmp_val = output.val[0];
        // printf("\nDev: %s\tType: %s\n", sensor_radtemp_t->name,
        //         saul_class_to_str(sensor_radtemp_t->driver->type));
        //phydat_dump(&output, dim);
    } else {
        DEBUG("[ERROR] Failed to read Radient Temperature\n");
    }

    /* Temperature 1-dim */
    dim = saul_reg_read(sensor_temp_t, &output); /* 15ms */
    if (dim > 0) {
        m->hdc_temp = output.val[0];
        // printf("\nDev: %s\tType: %s\n", sensor_temp_t->name,
        //         saul_class_to_str(sensor_temp_t->driver->type));
        //phydat_dump(&output, dim);
    } else {
        DEBUG("[ERROR] Failed to read Temperature\n");
    }

    /* Humidity 1-dim */
    LED_ON;
    dim = saul_reg_read(sensor_hum_t, &output); /* 15ms */
    if (dim > 0) {
        m->hdc_hum = output.val[0];
        // printf("\nDev: %s\tType: %s\n", sensor_hum_t->name,
        //         saul_class_to_str(sensor_hum_t->driver->type));
        //phydat_dump(&output, dim);
    } else {
        DEBUG("[ERROR] Failed to read Humidity\n");
    }
    LED_OFF;

    /* Time from start */
    m->uptime = xtimer_usec_from_ticks64(xtimer_now64());

    /* Others */
    m->serial = *fb_device_id;
    m->type   = TYPE_FIELD;
    m->flags  = PROVIDED_FLAGS;

    //puts("\n##########################");
}


int main(void)
{
    sensor_config();

    xtimer_usleep(OPENTHREAD_INIT_TIME);
    start_sendloop();

    return 0;
}
