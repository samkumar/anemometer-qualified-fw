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
} measure_set_t; //82 bytes

kernel_pid_t start_sendloop(void);

#define SENDER_PORT 49998
//#define RECEIVER_IP "2001:470:4a71:0:c46e:c7e9:6ac8:fcbd"
#define RECEIVER_IP "2001:470:4a71:0:9911:2f67:3553:2d7f"
#define RECEIVER_PORT 50000

/* Used for network benchmarks/testing. */
#define SEND_FAKE_DATA 1


/* Constants for buffering and chunking. */
// READING_BUF_SIZE must be a power of 2
#define READING_BUF_SIZE 64

#ifdef SEND_IN_BATCHES

#define READING_SEND_LIMIT 32

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
