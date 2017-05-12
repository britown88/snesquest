#include <string.h>
#include <stdio.h>

#include "Tools.h"
#include "libutils/CheckedMemory.h"

static void _showHelp() {
   printf(
      "snesgen usage: [tool] [files]\n"
      "   sid [directory]\n"
      "   dbgen [dbh file]\n"
      );
}

int main(int argc, char *argv[]) {
   if (argc <= 1) {
      _showHelp();
      return;
   }

   if (!strcmp(argv[1], "sid")) {
      if (argc <= 2) {
         _showHelp();
         return;
      }

      runSIDProcessing(argv[2]);
   }
   else if (!strcmp(argv[1], "dbgen")) {
      if (argc <= 2) {
         _showHelp();
         return;
      }

      runDBGen(argv[2]);
   }

   printMemoryLeaks();
}