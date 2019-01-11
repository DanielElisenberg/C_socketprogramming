/* commonfunctions.c
 *******************************************
 * Functions used by both server.c and
 * client.c.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "colors.h"

/* Takes a string and prints it as a
 * line in cyan letters with the
 * ID of the process that writes
 * before the string.
 * Only prints the string if the
 * program was running in debug mode.
 *
 * Input:
 *     char* string
 * Return:
 *     void
 */
void debugPrint(char *string, int debug){
    if(debug){
        printf(COLOR_CYAN ">>%d<< %s" COLOR_RESET, getpid(), string);
        printf("\n");
    }
}
/* Takes a string and prints it as a
 * line in red letters with the
 * ID of the process that writes
 * before the string.
 *
 * Input:
 *     char* string
 * Return:
 *     void
 */
void errorPrint(char *string){
    printf(COLOR_RED ">>%d<< %s" COLOR_RESET, getpid(), string);
    printf("\n");
}
/* Gets the checksum of a given string
 * by calculating the sum of all the
 * characters in the string and then
 * returning the sum % 32.
 *
 * Input:
 *     string: string of characters
 *     length: integer describing
 *             length of string
 * Return:
 *     Returns the result as
 *     an integer.
 */
int getChecksum(char *string, int length){
    int sum = 0;
    for(int i = 0; i < length; i++){
        unsigned char curr = (unsigned char)string[i];
        sum = sum + (int)curr;
    }
    return sum % 32;
}
/* Check if the requested port is
 * within reasonable bounds.
 * A number between 1 and 65536
 * is considered reasonable.
 *
 * Input:
 *     port: char* portnumber
 * Returns:
 *     0 if port is reasonable
 *    -1 if unreasonable
 */
int portCheck(char* port){
	int port_num = atoi(port);
	if(port_num > 0 && port_num < 65536){
		return 0;
	}else{
		return -1;
	}
}
