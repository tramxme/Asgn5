#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include "minfs.h"

/* This function checks if a partition Table is valid */
int validPT(FILE *image, uint32_t offset){
   uint8_t b510, b511;
   int res;

   if ((res = fseek(image, offset, SEEK_SET)) < 0){
      perror("fseek failed");
      return EXIT_FAILURE;
   }

   fread(&b510, sizeof(uint8_t), 1, image);
   fread(&b511, sizeof(uint8_t), 1, image);

   return b510 == VALID_PT_510 && b511 == VALID_PT_511;
}

/*This function returns the inode structure at a given inode number*/
inode *getInode(FILE *in, uint32_t offset, superblock *sb, uint32_t inode_num) {
   inode *curr = calloc(sizeof(inode), 1);

   /*Seek to requested inode*/
   fseek(in, offset + (sb->i_blocks + sb->z_blocks + 2) * sb->blocksize +
          (inode_num - 1) * INODE_SIZE, SEEK_SET);
   fread(curr, sizeof(inode), 1, in);

   return curr;
}

/* This function return a pointer to a list of directory entries
 * seeking using zone number */
dir_entry *getDir(FILE *in, uint32_t offset, uint32_t zoneNum,
      uint32_t zonesize, int dirNum){
   dir_entry *dirEntry = calloc(sizeof(dir_entry), dirNum);

   fseek(in, offset + zoneNum * zonesize, SEEK_SET);
   fread(dirEntry, sizeof(dir_entry), dirNum, in);

   return dirEntry;
}

/* This function check if a file in a list of directories
 * return the inode number or -1
 */
int hasFile(dir_entry *dirEntry, int dirNum, char *name){
   int i;

   for(i = 0; i < dirNum; i++){
      //printf("inode %d - name %s\n", dirEntry[i].inode, dirEntry[i].name);
      if (dirEntry[i].inode != 0 && !strcmp((char *) dirEntry[i].name, name)){
         return dirEntry[i].inode;
      }
   }
   return -1;
}

void printStuff(char *str, uint32_t num, int width){
   printf("  %s %*u\n", str, width - (int) strlen(str), num);
}

/* This function returns the information contain in the superblock */
void printSuperblock(superblock *sp){
   uint32_t zonesize = sp->blocksize << sp->log_zone_size;

   printf("Superblock Contents:\n");
   printf("Stored Fields:\n");
   printStuff("ninodes", sp->ninodes, SB_FORMAT_WIDTH);
   printStuff("i_blocks", sp->i_blocks, SB_FORMAT_WIDTH);
   printStuff("z_blocks", sp->z_blocks, SB_FORMAT_WIDTH);
   printStuff("firstdata", sp->firstdata, SB_FORMAT_WIDTH);
   printf("  %s %*d (zone size: %d)\n", "log_zone_size",
         SB_FORMAT_WIDTH - (int) (strlen("log_zone_size")),
           sp->log_zone_size, zonesize);
   printStuff("max_file", sp->max_file, SB_FORMAT_WIDTH);
   printf("  %-*s %#04x\n", MAGIC_FORMAT_WIDTH, "magic", sp->magic);
   printStuff("zones", sp->zones, SB_FORMAT_WIDTH);
   printStuff("blocksize", sp->blocksize, SB_FORMAT_WIDTH);
   printStuff("subversion", sp->subversion, SB_FORMAT_WIDTH);
}

/* This function returns the permission string */
char *getPerm(int num){
   char *res = calloc(11,1);
   int i = 0;

   res[i++] = num & DIR_MASK ? 'd' : '-';
   res[i++] = num & OWNER_RD_MASK ? 'r' : '-';
   res[i++] = num & OWNER_WR_MASK ? 'w' : '-';
   res[i++] = num & OWNER_EXE_MASK ? 'x' : '-';
   res[i++] = num & GROUP_RD_MASK ? 'r' : '-';
   res[i++] = num & GROUP_WR_MASK ? 'w' : '-';
   res[i++] = num & GROUP_EXE_MASK ? 'x' : '-';
   res[i++] = num & OTHER_RD_MASK ? 'r' : '-';
   res[i++] = num & OTHER_WR_MASK ? 'w' : '-';
   res[i++] = num & OTHER_EXE_MASK ? 'x' : '-';
   res[i] = '\0';

   return res;
}

/* This function prints the information contain in the inode */
void printInode(inode *fInode){
   char *perm = getPerm(fInode->mode);
   time_t curtime;

   time(&curtime);
   printf("File inode:\n"
         "  uint16_t mode 0x%04x (%s)\n"
         "  uint16_t links %d\n"
         "  uint16_t uid %d\n"
         "  uint16_t gid %d\n"
         "  uint32_t size %d\n"
         "  uint32_t atime %u --- %s" //TODO Add time
         "  uint32_t mtime %u --- %s" //TODO Add time
         "  uint32_t ctime %u --- %s\n" //TODO Add time
         "  Direct zones:\n"
         "              zone[0]   = %d\n"
         "              zone[1]   = %d\n"
         "              zone[2]   = %d\n"
         "              zone[3]   = %d\n"
         "              zone[4]   = %d\n"
         "              zone[5]   = %d\n"
         "              zone[6]   = %d\n"
         "  uint32_t    indirect %d\n"
         "  uint32_t    double %d\n"
         , fInode->mode, perm, fInode->links, fInode->uid,
         fInode-> gid, fInode->size, fInode->atime, ctime(&curtime),
         fInode->mtime, ctime(&curtime), fInode-> ctime, ctime(&curtime),
         fInode->zone[0], fInode->zone[1], fInode->zone[2], fInode->zone[3],
         fInode->zone[4], fInode->zone[5], fInode->zone[6],
         fInode->indirect, fInode->two_indirect);

         free(perm);
}





