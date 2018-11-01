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
