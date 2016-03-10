#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "minfs.h"

int validPT(pt_entry *pt){
   int i, j;
   i = *((char *) pt + BYTE510);
   j = *((char *) pt + BYTE511);
   return (i == VALID_PT_510 && j == VALID_PT_511);
}

void printSuperblock(superblock *sp){
   uint16_t zonesize = sp->blocksize << sp->log_zone_size;
   printf("Superblock Content:\n"
          "Stored Fields:\n"
          "  ninodes %d\n"
          "  i_blocks %d\n"
          "  z_blocks %d\n"
          "  firstdata %d\n"
          "  log_zone_size %d (zone size: %d)\n"
          "  max_file %d\n"
          "  magic 0x%04x\n"
          "  zones %d\n"
          "  blocksize %d"
          "  subversion %d\n", sp->ninodes, sp->i_blocks, sp->z_blocks,
          sp->firstdata, sp->log_zone_size, zonesize, sp->max_file, sp->magic,
          sp->zones, sp->blocksize, sp->subversion);
}

char* getPerm(int num){
   char *res = "----------", *temp;
   temp = res;

   if (num & DIR_MASK){
      *temp++ = 'd';
   }
   if (num & OWNER_RD_MASK){
      *temp++ = 'r';
   }
   if (num & OWNER_WR_MASK){
      *temp++ = 'w';
   }
   if (num & OWNER_EXE_MASK){
      *temp++ = 'x';
   }
   if (num & GROUP_RD_MASK){
      *temp++ = 'r';
   }
   if (num & GROUP_WR_MASK){
      *temp++ = 'w';
   }
   if (num & GROUP_EXE_MASK){
      *temp++ = 'x';
   }
   if (num & OTHER_RD_MASK){
      *temp++ = 'r';
   }
   if (num & OTHER_WR_MASK){
      *temp++ = 'w';
   }
   if (num & OTHER_EXE_MASK){
      *temp++ = 'x';
   }
   return res;
}

void printInode(inode *fInode){
   printf("File inode:\n"
          "  uint16_t mode 0x%04x (%s)\n"
          "  uint16_t links %d\n"
          "  uint16_t uid %d\n"
          "  uint16_t gid %d\n"
          "  uint32_t size %d\n"
          "  uint32_t atime %d --- \n" //TODO Add time
          "  uint32_t mtime %d --- \n" //TODO Add time
          "  uint32_t ctime %d --- \n\n" //TODO Add time
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
          , fInode->mode, getPerm(fInode->mode), fInode->links, fInode->uid,
          fInode-> gid, fInode->size, fInode->atime, fInode->mtime,
          fInode-> ctime, fInode->zone[0], fInode->zone[1], fInode->zone[2],
          fInode->zone[3], fInode->zone[4], fInode->zone[5], fInode->zone[6],
          fInode->indirect, fInode->two_indirect);
}

void printDir(){
}

int main(int argc, char **argv){
   int v = 0, h = 0, p = 0, s = 0;
   int part = -1, sub = -1;
   char imagefile[MAX_LENGTH], path[MAX_LENGTH];
   char *ptr;
   int c, res;
   FILE *image;
   char *usage = "usage: minls   [ -v ] [ -p num ] ] imagefile [ path ]\n"
      "Options:\n"
      "-p   part    --- select partition for filesystem (default: none)\n"
      "-s   sub     --- select subpartition for filesystem (default: none)\n"
      "-h   help    --- print usage information and exit\n"
      "-v   verbose --- increase verbosity level";

   pt_entry *pt = calloc(sizeof(pt_entry), 4);
   superblock *sBlock = calloc(sizeof(superblock), 1);

   //TODO: error catching when necessary info is not provided
   if (argc == 1){
      printf("%s\n", usage);
      return 0;
   }
   else {
      while ((c = getopt(argc, argv, "vhp:s:")) > 0){
         switch (c){
            case 'v':
               v = 1;
               break;
            case 'h':
               h = 1;
               printf("%s\n", usage);
               return 0;
            case 'p':
               p = 1;
               part = strtol(optarg, &ptr, 10);
               if (part >= MAX_PART){
                  perror("Cannot have more than 4 partitions");
                  return EXIT_FAILURE;
               }
               break;
            case 's':
               s = 1;
               sub =  strtol(optarg, &ptr, 10);
               if (part < 0 && sub >= 0){
                  perror("You cannot have subpartition without partition");
                  return EXIT_FAILURE;
               }

               if (sub >= MAX_PART){
                  perror("Cannot have more than 4 partitions");
                  return EXIT_FAILURE;
               }
               break;
         }
      }

      strcpy(imagefile, argv[optind]); /* Get imagefile */

      /* Get the path if provided */
      if (++optind < argc){
         strcpy(path, argv[optind]);
      }
   }

   image = fopen(imagefile, "r");

   /* Partitioned image */
   if (part >= 0){
      if ((res = fseek(image, PART_OFFSET, SEEK_SET)) < 0){
         perror("fseek failed");
         return EXIT_FAILURE;
      }

      fwrite(pt, sizeof(pt_entry), 4, image);

      /* Validate the partition table */
      if (!validPT(pt)){
         perror("Not valid parition table");
         return EXIT_FAILURE;
      }

      /* Validate MINIX parition */
      if (pt[part].type == MINIX_PART){
         printf("This is minix partition\n");
      }
      else{
         perror("This is not a MINIX partition\n");
         return EXIT_FAILURE;
      }
   }
   /* If no partition specified */
   else {
      if ((res = fseek(image, SUPERBLOCK_OFFSET, SEEK_SET)) < 0){
         perror("fseek failed");
         return EXIT_FAILURE;
      }

      fread(sBlock, sizeof(superblock), 1, image);

      printf("sblock magic %04x\n", sBlock->magic);

      if (sBlock->magic != MINIX_MAGIC){
         printf("Bad magic number. (0x%04x)\n", sBlock->magic);
         printf("This doesn't look like a MINIX filesystem.\n");
         return EXIT_FAILURE;
      }
   }

   free(pt);
   free(sBlock);
   fclose(image);
   return 0;
}
