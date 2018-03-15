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
#include "asic.h"
#include "mutex.h"
#include "cond.h"

#include "anemometer.h"


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

// 1 second, defined in us
#define INTERVAL (1000000U)
#define NETWORK_RTT_US 1000000
#define COUNT_TX (-4)
#define A_LONG_TIME 5000000U
#define MAIN_QUEUE_SIZE     (8)

#define OPENTHREAD_INIT_TIME 5000000ul

#define DIRECT_DATA_ADDRESS 0x65

//Anemometer v2
#define L7_TYPE 9

/* Variables for communicating with TCP thread. */
extern measure_set_t reading;
extern bool reading_ready;
extern mutex_t ready_mutex;
extern cond_t ready_cond;

uint16_t ms_seqno = 0;

void reboot(void){
  NVIC_SystemReset();
}
//In a 64 byte array of 16 big endian int16_t pairs of I and Q, what
//is the index where the complex magnitude is greatest [0,15]?
uint8_t calculate_max_index(uint8_t *data, uint8_t print) {
  int64_t ix;
  int64_t qx;
  uint64_t magsqr;
  uint64_t magsqrmax;
  uint8_t indexmax;

  magsqrmax = 0;
  indexmax = 0;

  for (int i = 0; i < 16; i++) {
    qx = (int16_t) ((uint16_t)data[i<<2] + (((uint16_t)data[(i<<2) + 1]) << 8));
    ix = (int16_t) ((uint16_t)data[(i<<2) + 2] + (((uint16_t)data[(i<<2) + 3]) << 8));
    magsqr = (qx * qx) + (ix * ix);
    if (print) {
      printf("%8lx ", (long unsigned int)magsqr);
    }
    if (magsqr > magsqrmax) {
      indexmax = i;
      magsqrmax = magsqr;
    }
  }
  return indexmax;
}


saul_reg_t *sensor_temp_t    = NULL;
saul_reg_t *sensor_hum_t     = NULL;
saul_reg_t *sensor_mag_t     = NULL;
saul_reg_t *sensor_accel_t   = NULL;


void sensor_config(void) {

    sensor_mag_t     = saul_reg_find_type(SAUL_SENSE_MAG);
    if (sensor_mag_t == NULL) {
		  DEBUG("[ERROR] Failed to init MAGNETIC sensor\n");
  	} else {
  		DEBUG("MAGNETIC sensor OK\n");
  	}

    sensor_accel_t   = saul_reg_find_type(SAUL_SENSE_ACCEL);
    if (sensor_accel_t == NULL) {
		  DEBUG("[ERROR] Failed to init ACCEL sensor\n");
  	} else {
  		DEBUG("ACCEL sensor OK\n");
  	}

}

void tx_measure(asic_tetra_t *a, measurement_t *m)
{
  measure_set_t ms;
  phydat_t output; /* Sensor output data (maximum 3-dimension)*/
  int dim;         /* Demension of sensor output */
  uint8_t parity;
  parity = 0;
  ms.l7type = L7_TYPE;
  ms.type = 0;
  ms.buildnum = BUILD;
  ms.primary = m->primary;
  ms.seqno = ms_seqno++;
  #if defined(DUCT6_TYPE)
  if (m->primary == 5) {
    ms_seqno += 2; //for 6 channel skip seqno % 8 == 6/7
  }
  #endif
  ms.parity = 0;

  /* Magnetic field 3-dim */
  dim = saul_reg_read(sensor_mag_t, &output);
  if (dim > 0) {
      ms.mag_x = output.val[0]; ms.mag_y = output.val[1]; ms.mag_z = output.val[2];
  } else {
      DEBUG("[ERROR] Failed to read magnetic field\n");
  }

  /* Acceleration 3-dim */
  dim = saul_reg_read(sensor_accel_t, &output);
  if (dim > 0) {
      ms.acc_x = output.val[0]; ms.acc_y = output.val[1]; ms.acc_z = output.val[2];
  } else {
      printf("[ERROR] Failed to read Acceleration\n");
  }

  for (int i = 0; i < 6; i++) {
    printf("calred %d = %d\n", i, a->calres[i]);
  }
  //
  // for (int i = 0; i < 16; i++) {
  //     printf("%8u ", i);
  // }
  // printf(" =======\n");
  for(int i = 0;i<3;i++) {
    uint8_t maxindex = calculate_max_index(m->sampledata[i], 0);
    //printf(" p=%d m[%d] = %d\n", m->primary, i, maxindex);
    ms.max_index[i] = maxindex;
    if (maxindex <= 3) {
      maxindex = 0;
    } else {
      maxindex -= 3;
    }
    //Copy 4 IQ pairs starting from 3 before the max index, unless the max is right
    //at the beginning, then start from 0
    memcpy(&(ms.data[i][0]), &(m->sampledata[i][maxindex<<2]), 16);
  }

  //Send the calibration result for the primary
  ms.cal_res = a->calres[m->primary];
  //ms.type = msi+10;

  //Calculate the parity
  for (int i = 0; i < sizeof(measure_set_t); i++) {
    parity ^= ((uint8_t*)&ms)[i];
  }
  ms.parity = parity;

  mutex_lock(&ready_mutex);
  memcpy(&reading, &ms, sizeof(reading));
  reading_ready = true;
  cond_signal(&ready_cond);
  mutex_unlock(&ready_mutex);

  //Also write the packet to I2C_0
  i2c_write_bytes(I2C_0, DIRECT_DATA_ADDRESS, "cafebabe",8);
  i2c_write_bytes(I2C_0, DIRECT_DATA_ADDRESS, (uint8_t*)&(ms),sizeof(measure_set_t));
  for (int i = 0; i < 3; i++) {
    i2c_write_bytes(I2C_0, DIRECT_DATA_ADDRESS, (uint8_t*)&(m->sampledata[i][0]), 64);
  }
}

void initial_program(asic_tetra_t *a)
{
  int8_t e;
  e = (int)asic_init(a, I2C_0);
  printf("[init] asic_init return with %d\n", e);

  asic_led(a, 1, 1, 1);
  printf("[init] first errcode was %d\n", e);
  uint8_t bad = 0;
  for (int i = 0; i < NUMASICS; i ++)
  {
    e = asic_program(a, i);
    printf("[init] program pass 1 for %d code was %d\n", i, e);
  }
  for (int i = 0; i < NUMASICS; i ++)
  {
    e = asic_program(a, i);
    if (e) bad = 1;
    printf("[init] program pass 2 for %d code was %d\n", i, e);
  }
  asic_led(a, 1,0,1);
  xtimer_usleep(100000); //100ms
  for (int i = 0; i < NUMASICS; i ++)
  {
    e = asic_configure(a, i);
    if (e) bad = 1;
    printf("[init] configure for %d code was %d\n", i, e);
  }
  if (bad) {
    asic_led(a, 1,0,0);
    xtimer_usleep(A_LONG_TIME);
    reboot();
  }
  asic_led(a, 1,1,1);
  xtimer_usleep(100000); //100ms
  asic_all_out_of_reset(a);
  xtimer_usleep(100000); //100ms
  bad = 0;
  for (int i = 0; i < NUMASICS; i ++)
  {
    e = asic_check_ready(a, i);
    if (e) bad = 1;
    printf("[init] %d ready was %d\n", i, e);
  }
  if (bad) {
    asic_led(a, 1,0,0);
    xtimer_usleep(A_LONG_TIME);
    reboot();
  }
  asic_led(a, 0,1,0);
  xtimer_usleep(100000); //100ms
}
#if 0
void dump_measurement(asic_tetra_t *a, measurement_t *m)
{
  printf("primary: %d\n", m->primary);
  for (int8_t num = 0; num < 4; num ++)
  {
    int16_t iz[16];
    int16_t qz[16];
    uint64_t magsqr[16];
    uint64_t magmax = 0;
    uint16_t tof_sf;
    uint8_t *b = &m->sampledata[num][0];
    tof_sf = b[0] + (((uint16_t)b[1]) << 8);
    for (int i = 0; i < 16; i++)
    {
      qz[i] = (int16_t) (b[6+i*4] + (((uint16_t)b[6+ i*4 + 1]) << 8));
      iz[i] = (int16_t) (b[6+i*4 + 2] + (((uint16_t)b[6+ i*4 + 3]) << 8));
      magsqr[i] = (uint64_t)(((int64_t)qz[i])*((int64_t)qz[i]) + ((int64_t)iz[i])*((int64_t)iz[i]));
      if (magsqr[i] > magmax)
      {
        magmax = magsqr[i];
      }
    }
    //Now we know the max, find the first index to be greater than half max
    uint64_t quarter = magmax >> 2;
    int ei = 0;
    int si = 0;
    for (int i = 0; i < 16; i++)
    {
      if (magsqr[i] < quarter)
      {
        si = i;
      }
      if (magsqr[i] > quarter)
      {
        ei = i;
        break;
      }
    }
    double s = sqrt((double)magsqr[si]);
    double e = sqrt((double)magsqr[ei]);
    double h = sqrt((double)quarter);
    double freq = tof_sf/2048.0*a->calres[num]/a->cal_pulse;
    double count = si + (h - s)/(e - s);
    double tof = (count + COUNT_TX) / freq * 8;

    //Now "linearly" interpolate
    printf("count %d /1000\n", (int)(count*1000));
    printf("tof_sf %d\n", tof_sf);
    printf("freq %d uHz\n", (int)(freq*1000));
    printf("tof %d uS\n", (int)(tof*1000));
    printf("tof 50us estimate %duS\n", (int)(count*50));
    for (int i = 0; i < 16; i++)
    {
      printf("data %d = %d + %di\n", i, qz[i], iz[i]);
    }
    printf(".\n");
  }
}
#endif
measurement_t sampm[4];
void begin(void)
{
  sensor_config();
  asic_tetra_t a;
  int8_t e;
  initial_program(&a);
  xtimer_usleep(100000);
  e = asic_calibrate(&a);
  if (e) {
    printf("calibrate failed\n");
    goto failure;
  }
  while (1)
  {
    for (int i = 0; i < 128; i++)
    {
      e = asic_fake_measure(&a);
      if (e) goto failure;
      for (int p = 0; p < NUMASICS; p ++)
      {
        e = asic_measure_just_iq(&a, p, &sampm[p]);
        if(e) goto failure;
      }
      for (int p = 0; p < NUMASICS; p ++)
      {
        tx_measure(&a, &sampm[p]);
      }
      xtimer_usleep(50000);
    }
  }
failure:
  asic_led(&a, 1,1,0);
  printf("[run] encountered failure\n");
  xtimer_usleep(A_LONG_TIME);
  reboot();
}
int main(void)
{
    printf("[init] booting build b%d\n",BUILD);
    xtimer_usleep(OPENTHREAD_INIT_TIME);
    start_sendloop();
    begin();

    return 0;
}
