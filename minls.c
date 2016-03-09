#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "const.h"
#include "minixStruct.h"

int validPT(partition_table *pt){
   int *i, *j;
   *i = *((char *) pt + BYTE510);
   *j = *((char *) pt + BYTE511);
   return (*i == SIG1 && *j == SIG2);
}

int main(int argc, char **argv){
   int v = 0, h = 0, p = 0, s = 0;
   int part, sub;
   char imagefile[MAX_LENGTH], path[MAX_LENGTH];
   int c, res;
   FILE *image;
   char *usage = "usage: minls   [ -v ] [ -p num ] ] imagefile [ path ]\n"
      "Options:\n"
      "-p   part    --- select partition for filesystem (default: none)\n"
      "-s   sub     --- select subpartition for filesystem (default: none)\n"
      "-h   help    --- print usage information and exit\n"
      "-v   verbose --- increase verbosity level";

   partition_table *pt = calloc(sizeof(partition_table), 1);

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
               part = (int) optarg;
               break;
            case 's':
               s = 1;
               sub = (int) optarg;
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
   if ((res = fseek(image, PTLOC, SEEK_SET)) < 0){
      perror("fseek failed");
      return EXIT_FAILURE;
   }

   fwrite(pt, sizeof(partition_table), 1, image);

   if (!validPT(pt)){
      perror("Not valid parition table");
      return EXIT_FAILURE;
   }

   /*
   printf("Open file successfully\n");
   printf("type partition %x\n", pt->type);
   printf("size %d\n", pt->size);
   */

   /*
   if (pt->type == MINIX_PART){
      printf("This is minix partition\n");
   }
   else{
      perror("This is not a MINIX partition\n");
      return EXIT_FAILURE;
   }
   */

   fclose(image);
   return 0;
}
