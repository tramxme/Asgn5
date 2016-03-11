#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
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

void printStuff(char *str, uint32_t num, int width){
   printf("  %s %*u\n", str, width - (int) strlen(str), num);
}

/* This function returns the information contain in the superblock */
void printSuperblock(superblock *sp){
   uint16_t zonesize = sp->blocksize << sp->log_zone_size;

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

/* This function return a pointer to a list of directory entries
 * seeking using zone number */
dir_entry *getDir(FILE *in, uint32_t offset, superblock *sb,
      uint32_t zoneNum, uint16_t zonesize, int dirNum){
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
      if (dirEntry[i].inode != 0 && !strcmp((char *) dirEntry[i].name, name)){
         return dirEntry[i].inode;
      }
   }
   return EXIT_FAILURE;
}

int printFiles(FILE *in, superblock *sb, uint32_t offset, char *path,
   inode *fInode, uint16_t zonesize){

   int dirNum = fInode->size/DIR_ENTRY_SIZE;
   dir_entry *dirEntry = getDir(in, offset, sb, fInode->zone[0],
         zonesize, dirNum);
   char *tempPath, *ptr = calloc(strlen(path), 1);
   uint32_t inodeNum;
   int isDir = 1;
   inode *curInode;

   strcpy(ptr, path);

   if (strcmp(path, "/")){
      strsep(&path, "/");
      while ((tempPath = strsep(&path, "/")) != '\0'){
         if (!isDir){
            return EXIT_FAILURE;
         }

         inodeNum = hasFile(dirEntry, dirNum, tempPath);
         if (inodeNum != EXIT_FAILURE){
            curInode = getInode(in, offset, sb, inodeNum);
    //        printf("temppath %s - inodeNum %d\n", tempPath, inodeNum);

            if (curInode->mode & REG_FILE_MASK){
               isDir = 0;
            }

            if (isDir)
            {
               free(dirEntry);
               dirNum = curInode->size/DIR_ENTRY_SIZE;
               dirEntry = getDir(in, offset, sb, curInode->zone[0], zonesize,
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

   ptr = calloc(strlen(path), 1);
   strcpy(ptr, path);
   if ((res = printFiles(image, sBlock, offset, ptr, Inode, zonesize)) != 0){
      fprintf(stderr, "%s: File not found\n", path);
      return EXIT_FAILURE;
   }

   if (v){
      printf("\n");
      printSuperblock(sBlock);
      printf("\n");
      printInode(Inode);
   }

   free(pt);
   free(ptr);
   free(subpt);
   free(sBlock);
   free(Inode);
   fclose(image);
   return 0;
}
