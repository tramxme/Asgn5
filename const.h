#include <stdint.h>

#define MAX_LENGTH 1000
#define PTLOC 0x1BE
#define BOOTABLE 0x80
#define MINIX_PART 0x81
#define BYTE510 510
#define BYTE511 511
#define SIG1 0x55
#define SIG2 0xAA



typedef struct {
   uint8_t bootind; /* Boot magic number */
   uint8_t start_head;
   uint8_t start_sec;
   uint8_t start_cyl;
   uint8_t type; /* Type of partition */
   uint8_t end_head;
   uint8_t end_sec;
   uint8_t end_cyl;
   uint8_t lFirst; /* First sector */
   uint8_t size; /* Size of partition */
}partition_table;
