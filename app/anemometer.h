#include <stdint.h>
#include <thread.h>

typedef struct __attribute__((packed))
{
  uint8_t   l7type;     // 0
  uint8_t   type;       // 1
  uint16_t  seqno;      // 2:3
  uint8_t   primary;    // 4
  uint8_t   buildnum;   // 5
  int16_t   acc_x;      // 6:7
  int16_t   acc_y;      // 8:9
  int16_t   acc_z;      // 10:11
  int16_t   mag_x;      // 12:13
  int16_t   mag_y;      // 14:15
  int16_t   mag_z;      // 16:17
  uint16_t  temp;        // 18:19
  int16_t   reserved;    // 20:21
  uint8_t   max_index[3]; // 22:24
  uint8_t   parity;    // 25
  uint16_t  cal_res;   // 26:27
  //Packed IQ data for 4 pairs
  //M-3, M-2, M-1, M
  uint8_t data[3][16];  //28:75
  uint16_t tof_sf[3];   //76:81
} measure_set_t; // 82 bytes

typedef struct __attribute__((packed))
{
    uint32_t seqno;                  // 0:3
    uint32_t msec;                   // 4:7
    uint64_t radio_on_time;          // 8:15
    uint64_t radio_off_time;         // 16:23
    uint64_t cpu_on_time;            // 24:31
    uint64_t cpu_off_time;           // 32:39
    uint32_t radio_link_tx;          // 40:43
    uint32_t radio_link_rx;          // 44:47
    uint32_t radio_tx_datareq;       // 48:51
    uint32_t radio_datareq_nopacket; // 52:55
    uint32_t packets_sent;           // 56:59
    uint32_t packets_received;       // 60:63
    uint32_t timeout_rexmits;        // 64:67
    uint32_t total_rexmits;           // 68:71
    uint32_t batches_sent;           // 72:75
    uint32_t batches_sent_sliding;   // 76:79
    uint8_t measures_app_queued;     // 80
    uint8_t bench_type;              // 81
} bench_set_t; // 82 bytes

kernel_pid_t start_sendloop(void);

#define SENDER_PORT 49998
//#define RECEIVER_IP "2001:470:4a71:0:c46e:c7e9:6ac8:fcbd"
#define RECEIVER_IP "2001:470:4a71:0:64f2:8807:bbfd:a8b4"
#define RECEIVER_PORT 50000

/* Used for network benchmarks/testing. */
#define SEND_FAKE_DATA 1


/* Constants for buffering and chunking. */
#ifdef USE_TCP
#define READING_BUF_SIZE 64
#endif
#ifdef USE_COAP
#define READING_BUF_SIZE 104
#endif

#ifdef SEND_IN_BATCHES

#define READING_SEND_LIMIT 64

#ifdef USE_TCP
// Make this large and let TCP perform segmentation
#define SEND_CHUNK_SIZE 1024
#endif
#ifdef USE_COAP
// This should be sized to CoAP uses the same number of link-layer frames as TCP
#ifdef USE_BLOCKWISE_TRANSFER
#define SEND_CHUNK_SIZE 512
#else
#define SEND_CHUNK_SIZE 469
#endif

#endif

#else

#define READING_SEND_LIMIT 1
#define SEND_CHUNK_SIZE (sizeof(measure_set_t))

#endif
