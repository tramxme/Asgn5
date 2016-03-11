#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "minfs.c"

/* This function prints a list of the files contained in a directory */
void printDir(FILE *in, uint32_t offset, superblock *sb,  dir_entry* dirEntry,
      int dirNum){
   inode *currInode;
   int i;

   /* For each directory, get inode and print info */
   for(i = 0; i < dirNum; i++){
      //printf("inode %d - name %s\n", dirEntry[i].inode, dirEntry[i].name);
      /* Validate inode number */
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

/* This function takes in the root inode and prints out
 * the contents of the provided path.
 * If the path is to a directory, it calls printDir.
 * If the path is to a file, it prints the permissions, size, and
 * name of the file.
 * If no path is given, it defaults to root directory.
 */
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

   /* Get the root directory entry */
   dir_entry *dirEntry = getDir(in, offset, fInode->zone[0],
         zonesize, dirNum);
   char *tempPath, *ptr = calloc(strlen(path) + 1, 1);
   uint32_t inodeNum;
   int isDir = 1;
   inode *curInode;

   strcpy(ptr, path);
 
   /* If path was provided */
   if (strcmp(path, "/")){
      if (path[0] == '/'){
         strsep(&path, "/");
      }
      /* Parse path to get individual directory names */
      while ((tempPath = strsep(&path, "/")) != '\0'){
         //printf("path %s\n", tempPath);

	 /* Stop checking once find a file */
         if (!isDir){
            return EXIT_FAILURE;
         }

	 /* Get the inode of each directory entry */
         inodeNum = hasFile(dirEntry, dirNum, tempPath);
         if (inodeNum != -1){
            curInode = getInode(in, offset, sb, inodeNum);

	    /* Check if directory or file */
            if (curInode->mode & REG_FILE_MASK){
               isDir = 0;
            }
            if (isDir) {
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

   /* Directory */
   if (isDir){
      printf("%s:\n", ptr);
      printDir(in, offset, sb, dirEntry, dirNum);
   }
   /* File */
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
   if (image == NULL) {
      perror("fopen");
      return EXIT_FAILURE;
   }

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

      if ((res = fread(pt, sizeof(pt_entry), MAX_PART, image)) != MAX_PART){
         if (ferror(image)) {
            perror("fread failed");
            return EXIT_FAILURE;
	 }
      }

      /*
      if ((res = fread(pt, sizeof(pt_entry), MAX_PART, image)) < 0){
         perror("fread failed");
         return EXIT_FAILURE;
      }
      */

      /* Validate MINIX partition */
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

         if ((res = fread(subpt, sizeof(pt_entry), MAX_PART, image)) !=
	   MAX_PART) {
	    if (ferror(image)) {
               perror("fread failed");
               return EXIT_FAILURE;
	    }
         }

         /* Validate MINIX subpartition */
         if (subpt[sub].type != MINIX_PART){
            perror("This is not a MINIX partition\n");
            return EXIT_FAILURE;
         }

         /* Get the filesystem */
         offset = subpt[sub].lFirst * SECTOR_SIZE;
      }
      else{
         /* Get the filesystem */
         if ((res = fseek(image, PART_OFFSET + pt[part].lFirst * SECTOR_SIZE
                     , SEEK_SET)) < 0){
            perror("fseek failed");
            return EXIT_FAILURE;
         }
      }
   }

   /* Get superblock */
   if ((res = fseek(image, SUPERBLOCK_OFFSET + offset, SEEK_SET)) < 0){
      perror("fseek failed");
      return EXIT_FAILURE;
   }

   if ((res = fread(sBlock, sizeof(superblock), 1, image)) != 1){
      if (ferror(image)) {
         perror("fread failed");
         return EXIT_FAILURE;
      }
   }

   /* Validate MINIX filesystem */
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

   if ((res = fread(Inode, sizeof(inode), 1, image)) != 1){
      if (ferror(image)) {
         perror("fread failed");
         return EXIT_FAILURE;
      }
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

   /* Print the files */
   ptr = calloc(strlen(path) + 1, 1);
   if (ptr == NULL) {
      perror("calloc");
//////What????
   }
   strcpy(ptr, path);
   if ((res = printFiles(image, sBlock, offset, ptr, Inode, zonesize)) != 0){
      fprintf(stderr, "%s: File not found\n", path);
      return EXIT_FAILURE;
   }

   /* Free allocated memory */
   free(pt);
   free(ptr);
   free(subpt);
   free(sBlock);
   free(Inode);
   fclose(image);
   return 0;
}
