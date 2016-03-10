#include <stdint.h>

/* Our Constants */
#define MAX_LENGTH      1000
#define MAX_PART        4
#define MAX_NAME_LEN    60
#define SUPERBLOCK_OFFSET 1024
#define FORMAT_WIDTH    9
#define SB_FORMAT_WIDTH 20
#define MAGIC_FORMAT_WIDTH 14

/*Minix Constants*/
#define BOOT_MAGIC      0x80
#define MINIX_PART      0x81
#define BYTE510         510
#define BYTE511         511
#define VALID_PT_510    0x55
#define VALID_PT_511    0xAA
#define PART_OFFSET     0x1BE
#define MINIX_MAGIC     0x4D5A
#define MINIX_MAGIC_REV 0x5A4D
#define INODE_SIZE      64
#define DIR_ENTRY_SIZE  64
#define SECTOR_SIZE     512

#define DIRECT_ZONES    7

/*Bit Masks (in octal)*/
#define FILE_MASK       0170000
#define REG_FILE_MASK   0100000
#define DIR_MASK        0040000
#define OWNER_RD_MASK   0000400
#define OWNER_WR_MASK   0000200
#define OWNER_EXE_MASK  0000100
#define GROUP_RD_MASK   0000040
#define GROUP_WR_MASK   0000020
#define GROUP_EXE_MASK  0000010
#define OTHER_RD_MASK   0000004
#define OTHER_WR_MASK   0000002
#define OTHER_EXE_MASK  0000001

typedef struct __attribute__((__packed__)) partition_table_entry {
   uint8_t bootind;       /*Boot magic number (0x80 if bootable)*/
   uint8_t start_head;    /*start of partition in CHS*/
   uint8_t start_sec;
   uint8_t start_cyl;
   uint8_t type;          /*type of partition (0x81 is Minix)*/
   uint8_t end_head;      /*end of partition in CHS*/
   uint8_t end_sec;
   uint8_t end_cyl;
   uint32_t lFirst;       /*first sector (LBA) -relative to disk*/
   uint32_t size;         /*size of partition (in sectors)*/
} pt_entry;

typedef struct __attribute__((__packed__)) superblock {
   uint32_t ninodes;       /*number inodes in this filesystem*/
   uint16_t pad1;          /*make things line up properly*/
   int16_t i_blocks;       /*# of blocks used by inode bit map*/
   int16_t z_blocks;       /*# of blocks used by zone bit map*/
   uint16_t firstdata;     /*number of first data zone*/
   int16_t log_zone_size;  /*log2 of blocks per zone*/
   int16_t pad2;           /*make things line up again*/
   uint32_t max_file;      /*maximum file size*/
   uint32_t zones;         /*number of zones on disk*/
   int16_t magic;          /*magic number*/
   int16_t pad3;           /*make things line up again*/
   uint16_t blocksize;     /*block size in bytes*/
   uint8_t subversion;      /*filesystem sub-version*/
} superblock;

typedef struct __attribute__((__packed__)) inode {
   uint16_t mode;          /*mode*/
   uint16_t links;         /*number of links*/
   uint16_t uid;
   uint16_t gid;
   uint32_t size;
   int32_t atime;
   int32_t mtime;
   int32_t ctime;
   uint32_t zone[DIRECT_ZONES];
   uint32_t indirect;
   uint32_t two_indirect;
   uint32_t unused;
} inode;

typedef struct __attribute__((__packed__)) directory_entry{
   uint32_t inode; /* inode number */
   unsigned char name[MAX_NAME_LEN]; /* Filename string */
}dir_entry;
