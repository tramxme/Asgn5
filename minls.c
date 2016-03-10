#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include "minfs.h"

/* This function checks if a partition Table is valid */
int validPT(FILE *image, uint16_t offset){
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

/* This function returns the information contain in the superblock */
void printSuperblock(superblock *sp){
   uint16_t zonesize = sp->blocksize << sp->log_zone_size;
   printf("Superblock Content:\n"
         "Stored Fields:\n"
         "  ninodes %u\n"
         "  i_blocks %d\n"
         "  z_blocks %d\n"
         "  firstdata %d\n"
         "  log_zone_size %d (zone size: %d)\n"
         "  max_file %u\n"
         "  magic 0x%04x\n"
         "  zones %d\n"
         "  blocksize %d\n"
         "  subversion %d\n", sp->ninodes, sp->i_blocks, sp->z_blocks,
         sp->firstdata, sp->log_zone_size, zonesize, sp->max_file, sp->magic,
         sp->zones, sp->blocksize, sp->subversion);
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

/*
void printFiles(FILE *in, char *path, inode *fInode, uint16_t zonesize){
   printf("%s\n", path);

   int i = 0;
   dir_entry *dirEntry = calloc(sizeof(dir_entry), fInode->size/zonesize + 1);

   for (i = 0; i < DIRECT_ZONES && fInode->zone[i] != 0; i++){
   }
}
*/

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
   pt_entry *subpt = calloc(sizeof(pt_entry), 4);
   superblock *sBlock = calloc(sizeof(superblock), 1);
   inode *Inode = calloc(sizeof(inode), 1);
   uint16_t zonesize;
   uint32_t offset = 0;

   //TODO: error catching when necessary info is not provided
   if (argc == 1){
      printf("%s\n", usage);
      return EXIT_FAILURE;
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
               return EXIT_FAILURE;
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
   if (p){
      if (!validPT(image, BYTE510)){
         perror("Not valid parition table - subpartition");
         return EXIT_FAILURE;
      }
      if ((res = fseek(image, PART_OFFSET, SEEK_SET)) < 0){
         perror("fseek failed");
         return EXIT_FAILURE;
      }

      fread(pt, sizeof(pt_entry), 4, image);

      /* Validate MINIX parition */
      if (pt[part].type != MINIX_PART){
         perror("This is not a MINIX partition\n");
         return EXIT_FAILURE;
      }

      offset = pt[part].lFirst * SECTOR_SIZE;

      /* If subpartition is provided */
      if (s){
         /* Validate the subpartition table */
         if (!validPT(image, pt[part].lFirst * SECTOR_SIZE + BYTE510)){
            perror("Not valid parition table - subpartition");
            return EXIT_FAILURE;
         }
         /* Get the subpartition table */
         if ((res = fseek(image, PART_OFFSET + pt[part].lFirst * SECTOR_SIZE,
                     SEEK_SET)) < 0){
            perror("fseek failed");
            return EXIT_FAILURE;
         }

         fread(subpt, sizeof(pt_entry), 4, image);

         /* Validate MINIX subparition */
         if (subpt[sub].type != MINIX_PART){
            perror("This is not a MINIX partition\n");
            return EXIT_FAILURE;
         }

         /* Get the file system */
         offset = subpt[sub].lFirst * SECTOR_SIZE;
      }
      else{
         /* Get the file system */
         if (( res = fseek(image, PART_OFFSET + pt[part].lFirst * SECTOR_SIZE
                     , SEEK_SET)) < 0){
            perror("fseek failed");
            return EXIT_FAILURE;
         }
      }
   }

   if ((res = fseek(image, SUPERBLOCK_OFFSET + offset, SEEK_SET)) < 0){
      perror("fseek failed");
      return EXIT_FAILURE;
   }

   fread(sBlock, sizeof(superblock), 1, image);

   if (sBlock->magic != MINIX_MAGIC){
      printf("Bad magic number. (1x%04x)\n", sBlock->magic);
      printf("This doesn't look like a MINIX filesystem.\n");
      return EXIT_FAILURE;
   }

   /* Find the root inode */
   if ((res = fseek(image, offset +
               (sBlock->i_blocks + sBlock->z_blocks + 2) * sBlock->blocksize,
               SEEK_SET)) < 0){
      perror("fseek failed");
      return EXIT_FAILURE;
   }

   printf("sizeof(inode) %ld\n", sizeof(inode));

   fread(Inode, sizeof(inode), 1, image);
   zonesize = sBlock->blocksize << sBlock->log_zone_size;

   if (v){
      printf("\n");
      printSuperblock(sBlock);
      printf("\n");
      printInode(Inode);
   }


   /*
   printInode(Inode);
   zonesize = sBlock->blocksize << sBlock->log_zone_size;


   if (strlen(path) == 0){
      strcpy(path, "/:\0");
   }

   printFiles(image, path, Inode, zonesize);
   */


   free(pt);
   free(subpt);
   free(sBlock);
   free(Inode);
   fclose(image);
   return 0;
}
