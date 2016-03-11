#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "minfs.c"

/* This function prints the directory info */
void printDir(FILE *in, uint32_t offset, superblock *sb,  dir_entry* dirEntry,
      int dirNum){
   inode *currInode;
   int i;

   for(i = 0; i < dirNum; i++){
      //printf("inode %d - name %s\n", dirEntry[i].inode, dirEntry[i].name);
      if (dirEntry[i].inode != 0){
         currInode = getInode(in, offset, sb, dirEntry[i].inode);
         printf("%s %*d %s\n", getPerm(currInode->mode), FORMAT_WIDTH,
               currInode->size, dirEntry[i].name);
         free(currInode);
      }
   }
}

uint32_t *getZones(FILE *in, uint32_t offset, uint16_t blocksize,
      inode *Inode, uint32_t numOfZones, uint32_t zonesize){
   int idx;

   uint32_t *zones = calloc(sizeof(uint32_t), numOfZones);
   /* Direct zone*/
   for (idx = 0; idx < DIRECT_ZONES; idx++){
      zones[idx] = Inode->zone[idx];
   }

   /* Indirect */
   if (numOfZones > DIRECT_ZONES &&
         numOfZones < DIRECT_ZONES + blocksize/sizeof(uint32_t)){
      fseek(in, offset + Inode->indirect * zonesize, SEEK_SET);
      fread(&zones[idx], sizeof(uint32_t), numOfZones - DIRECT_ZONES, in);
      idx += numOfZones - DIRECT_ZONES;
   }

   /* Double indirect */
   if (numOfZones > DIRECT_ZONES + blocksize/sizeof(uint32_t)){
   }

   return zones;
}

int printFiles(FILE *in, superblock *sb, uint32_t offset, char *path,
   inode *fInode, uint32_t zonesize){

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
   char *tempPath, *ptr = calloc(strlen(path) + 1, 1);
   uint32_t inodeNum;
   int isDir = 1;
   inode *curInode;

   strcpy(ptr, path);

   if (strcmp(path, "/")){
      if (path[0] == '/'){
         strsep(&path, "/");
      }
      while ((tempPath = strsep(&path, "/")) != '\0'){
         //printf("path %s\n", tempPath);
         if (!isDir){
            return EXIT_FAILURE;
         }

         inodeNum = hasFile(dirEntry, dirNum, tempPath);
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
      printf("%s:\n", ptr);
      printDir(in, offset, sb, dirEntry, dirNum);
   }
   else {
      printf("%s %*d %s\n", getPerm(curInode->mode), FORMAT_WIDTH,
            curInode->size, ptr);
   }

   free(dirEntry);
   free(ptr);

   return 0;
}

int main(int argc, char **argv){
   int v = 0, p = 0, s = 0;
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
   uint32_t zonesize;
   uint32_t offset = 0;

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

      if ((res = fread(pt, sizeof(pt_entry), 4, image)) < 0){
         perror("fread failed");
         return EXIT_FAILURE;
      }

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

         if ((res = fread(subpt, sizeof(pt_entry), 4, image)) < 0) {
            perror("fread failed");
            return EXIT_FAILURE;
         }

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
         if ((res = fseek(image, PART_OFFSET + pt[part].lFirst * SECTOR_SIZE
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

   if ((res = fread(sBlock, sizeof(superblock), 1, image)) < 0){
      perror("fread failed");
      return EXIT_FAILURE;
   }

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

   if ((res = fread(Inode, sizeof(inode), 1, image)) < 0){
      perror("fread failed");
      return EXIT_FAILURE;
   }

   zonesize = sBlock->blocksize << sBlock->log_zone_size;

   if (strlen(path) == 0){
      strcpy(path, "/\0");
   }

   if (v){
      printf("\n");
      printSuperblock(sBlock);
      printf("\n");
      printInode(Inode);
   }

   ptr = calloc(strlen(path) + 1, 1);
   strcpy(ptr, path);
   if ((res = printFiles(image, sBlock, offset, ptr, Inode, zonesize)) != 0){
      fprintf(stderr, "%s: File not found\n", path);
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
