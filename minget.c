#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "minfs.c"

int printFile(FILE *in, uint32_t offset, superblock *sb,
      inode *fInode, uint32_t zonesize,
      char *srcpath, char *dstPath){

   int dirNum = fInode->size/DIR_ENTRY_SIZE;
   int idx;
   uint32_t numZones = fInode->size / zonesize;

   if (fInode->size % zonesize){
      numZones += 1;
   }


   for (idx = 0; idx < numZones; idx++){
   }

   dir_entry *dirEntry = getDir(in, offset, fInode->zone[0],
         zonesize, dirNum);

   char *tempPath, *ptr = calloc(strlen(srcpath) + 1, 1);
   uint32_t inodeNum;
   int isDir = 1;
   inode *curInode;

   strcpy(ptr, srcpath);

   if (strcmp(srcpath, "/")){
      strsep(&srcpath, "/");
      while ((tempPath = strsep(&srcpath, "/")) != '\0'){
         if (!isDir){
            return EXIT_FAILURE;
         }

         inodeNum = hasFile(dirEntry, dirNum, tempPath);
         //printf("tempPath %s - inodeNum %d\n", tempPath, inodeNum);
         if (inodeNum != -1){
            curInode = getInode(in, offset, sb, inodeNum);

            if (curInode->mode & REG_FILE_MASK){
               isDir = 0;
            }

            if (isDir)
            {
               free(dirEntry);
               dirNum = curInode->size/DIR_ENTRY_SIZE;
               dirEntry = getDir(in, offset, curInode->zone[0], zonesize,
                     dirNum);
            }
         }
         else{
            return EXIT_FAILURE;
         }
      }
   }

   if (isDir){
      return -1;
   }
   else {
      fseek(in, offset + curInode->zone[0] * zonesize, SEEK_SET);
      tempPath = calloc(curInode->size, 1);
      fread(tempPath, 1, curInode->size, in);
      fwrite(tempPath, 1, curInode->size, stdout);
      free(tempPath);
   }

   free(dirEntry);
   free(ptr);

   return 0;
}

int main(int argc, char **argv){
   int v = 0, p = 0, s = 0;
   int part, sub;
   char imagefile[MAX_LENGTH], srcpath[MAX_LENGTH], dstpath[MAX_LENGTH];
   char *ptr;
   int c;
   FILE *image;
   char *usage = "usage: minget   "
      "[ -v ] [ -p num ] ] imagefile srcpath [ dstpath ]\n"
      "Options:\n"
      "-p   part    --- select partition for filesystem (default: none)\n"
      "-s   sub     --- select subpartition for filesystem (default: none)\n"
      "-h   help    --- print usage information and exit\n"
      "-v   verbose --- increase verbosity level";

   pt_entry *pt = calloc(sizeof(pt_entry), 4);
   pt_entry *subpt = calloc(sizeof(pt_entry), 4);
   superblock *sBlock = calloc(sizeof(superblock), 1);
   inode *Inode = calloc(sizeof(inode), 1);
   uint32_t zonesize;
   uint32_t offset = 0;

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
      strcpy(srcpath, argv[++optind]); /* Get srcpath */

      /* Get the dstpath if provided */
      if (++optind < argc){
         strcpy(dstpath, argv[optind]);
      }
   }

   image = fopen(imagefile, "r");

   /* Partitioned image */
   if (p){
      if (!validPT(image, BYTE510)){
         perror("Not valid parition table");
         return EXIT_FAILURE;
      }

      fseek(image, PART_OFFSET, SEEK_SET);

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
            perror("Not valid subparition table");
            return EXIT_FAILURE;
         }
         /* Get the subpartition table */
         fseek(image, PART_OFFSET + pt[part].lFirst * SECTOR_SIZE, SEEK_SET);

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
         fseek(image, PART_OFFSET + pt[part].lFirst * SECTOR_SIZE , SEEK_SET);
      }
   }

   fseek(image, SUPERBLOCK_OFFSET + offset, SEEK_SET);
   fread(sBlock, sizeof(superblock), 1, image);

   if (sBlock->magic != MINIX_MAGIC){
      printf("Bad magic number. (1x%04x)\n", sBlock->magic);
      printf("This doesn't look like a MINIX filesystem.\n");
      return EXIT_FAILURE;
   }

   /* Find the root inode */
   fseek(image, offset +
         (sBlock->i_blocks + sBlock->z_blocks + 2) * sBlock->blocksize,
         SEEK_SET);

   fread(Inode, sizeof(inode), 1, image);
   zonesize = sBlock->blocksize << sBlock->log_zone_size;

   if (strlen(dstpath) == 0){
      strcpy(dstpath, "/\0");
   }

   if (v){
      printf("\n");
      printSuperblock(sBlock);
      printf("\n");
      printInode(Inode);
   }

   ptr = calloc(strlen(srcpath) + 1, 1);
   strcpy(ptr, srcpath);

   if (printFile(image, offset, sBlock,Inode, zonesize, ptr, dstpath) != 0){
      fprintf(stderr, "%s: Not a regular file\n", srcpath);
      return EXIT_FAILURE;
   }



   free(pt);
   free(ptr);
   free(subpt);
   free(sBlock);
   free(Inode);
   fclose(image);
   return 0;
}
