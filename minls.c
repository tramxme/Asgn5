#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define MAX_LENGTH 1000

int main(int argc, char **argv){
   int v = 0, h = 0, p = 0, s = 0;
   int part, sub;
   char imagefile[MAX_LENGTH], path[MAX_LENGTH];
   int c;
   char *usage = "usage: minls   [ -v ] [ -p num ] ] imagefile [ path ]\n"
      "Options:\n"
      "-p   part    --- select partition for filesystem (default: none)\n"
      "-s   sub     --- select subpartition for filesystem (default: none)\n"
      "-h   help    --- print usage information and exit\n"
      "-v   verbose --- increase verbosity level";

   //TODO: error catching when necessary info is not provided
   if (argc == 1){
      printf("%s\n", usage);
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

   return 0;
}
