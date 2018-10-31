#include <stdint.h>
#include <thread.h>

typedef struct __attribute__((packed,aligned(4))) {
  uint16_t type;
  uint16_t serial;
  //From below is encrypted
  //We use a zero IV, so it is important that the first AES block
  //is completely unique, which is why we include uptime.
  //It is expected that hamilton nodes never reboot and that
  //uptime is strictly monotonic
  uint64_t uptime;
  uint16_t flags; //which of the fields below exist
  int16_t  acc_x;
  int16_t  acc_y;
  int16_t  acc_z;
  int16_t  mag_x;
  int16_t  mag_y;
  int16_t  mag_z;
  int16_t  tmp_die;
  int16_t  tmp_val;
  int16_t  hdc_temp;
  int16_t  hdc_hum;
  int16_t  light_lux;
  uint16_t buttons;
  uint16_t occup;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t reserved3;
} ham7c_t;

kernel_pid_t start_sendloop(void);

#define SENDER_PORT 49998
//#define RECEIVER_IP "2001:470:4a71:0:c46e:c7e9:6ac8:fcbd"
#define RECEIVER_IP "fdde:ad00:beef:0001::2"
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
#define SEND_CHUNK_SIZE (sizeof(ham7c_t))

#endif
