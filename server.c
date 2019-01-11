/* server.c
 * kandidatnummer: 15201
 *******************************************
 * USAGE:
 * Arguments: <filepath>  <port> -DEBUG
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <assert.h>

#include "colors.h"

/* Fields 		*/
FILE *f;
int client_socket;
int server_socket;
pid_t serverid;
int debug;
/* Functions 	*/
void usage(int argc, char* argv[]);
void createSocket(char* port);
void takeRequests();
int sendJob();
void errorCode(int type);
void sendTermSignal(int sig);
void outOfJobs();
void signalHandler(int sig);

void errorPrint(char* string);
void debugPrint(char* string, int debug);
int getChecksum(char* string, int length);
int portCheck(char* port);

/* Main-function
 * That sets up the jobfile, a networking socket
 * accepts a connection from one client and
 * communicates with client until either
 * terminates or experiences an error.
 *
 * Input:
 *     argc: amount of user arguments
 *     argv: user arguments
 * Return:
 *     Returns 0 on success
 *     -1 on error.
 *
 */
int main(int argc, char *argv[]) {
	serverid = getpid();
	debug = 0;

    struct sigaction sigint;
    memset(&sigint, 0, sizeof(sigint));
    sigint.sa_handler = signalHandler;
    sigaction(SIGINT, &sigint, NULL);

    usage(argc, argv);

    debugPrint("Opening job-file.", debug);
	f = fopen(argv[1], "r");
	if(f == NULL){
		errorPrint("Couldn't find the file. Shutting down server!");
		return 0;
	}

    debugPrint("Creating server-socket.", debug);
    createSocket(argv[2]);
    if(server_socket == -1){
        errorPrint("Error in setting up server socket!");
        debugPrint("Shutting down server...", debug);
        fclose(f);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t addr_len = sizeof(client_addr);
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);

    char ipstring[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ipstring, sizeof ipstring);
    printf(COLOR_CYAN">>%d<< Client (%s) - Connected to server.", serverid, ipstring);
    printf(COLOR_RESET "\n");
    close(server_socket);

	takeRequests();

	debugPrint("Shutting down server...", debug);
	fclose(f);
    close(client_socket);
    return 0;
}
/* How the program treats
 * arguments from user.
 *
 * Input:
 *     argc: amount of args
 *     argv: pointer to the args
 * Return:
 *     void
 *
 */
void usage(int argc, char* argv[]){
    if(argc < 3){
        errorPrint("Not enough program arguments supplied.\n");
        errorPrint("./server <joblist> <Port>.\n");
        errorPrint("To run client in debug mode add last argument '-DEBUG'\n");
        exit(EXIT_FAILURE);
    }
    if(portCheck(argv[2]) == -1){
        errorPrint("Please choose a port from 1 to 6535.");
        errorPrint("./server <joblist> <Port>.\n");
        errorPrint("To run client in debug mode add last argument '-DEBUG'\n");
        exit(EXIT_FAILURE);
    }
    if(argc == 4){
        char comp[7]= "-DEBUG";
        if(strcmp(comp, argv[3]) == 0){
            debug = 1;
            debugPrint("Running client in debug mode.", debug);
        }else{
            errorPrint("./server <joblist> <Port>.\n");
            errorPrint("To run client in debug mode add last argument '-DEBUG'\n");
            exit(EXIT_FAILURE);
        }
    }
}

/* Creates a networking socket with
 * the port given by the user. Binds the
 * socket and listens for client to connect.
 * After client connects we close
 * the server_socket as to not
 * accept further connections from
 * other clients.
 * Uses sockopt to make the address
 * Reusable after shutdown.
 *
 * Input:
 *     port:    port number
 * Return:
 *     If successful; returns the socket
 *     as an integer.
 *     -1 on any error
 */
void createSocket(char* port){
    int port_num = atoi(port);

    if(debug){
        printf(COLOR_CYAN ">>%d<< Port: %d", serverid, port_num);
        printf(COLOR_RESET"\n");
    }

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_num);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int))) {
        errorPrint("Error when attempting to make server-address reusable.");
        server_socket = -1;
        return;
    }
    int permission =
    bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    if(permission ==  -1){
        errorPrint("Error when binding port to port.");
        printf(COLOR_RED ">>>%d<<< Port: %d", getpid(), port_num);
        printf(COLOR_RESET"\n");
        server_socket = -1;
        return;
    }
    listen(server_socket, 1);
    debugPrint("Waiting for client to connect...", debug);

}

/* Loop that receives request from the
 * connected client, and sends a job
 * or multiple jobs in response.
 * Or terminates the server if the
 * client requests termination.
 * Message protocol described in
 * protokoll.txt.
 *
 * Input:
 *     none
 * Return:
 *     void
 */
void takeRequests(){
	int client_message;
	int numberOfJobs;

	while(1){
    	recv(client_socket, &client_message, sizeof(client_message), 0);
    	char request = client_message & 255;
    	switch(request){
    		case 'J':
    			numberOfJobs = (int)(client_message >> 8);
                if(debug == 1){
    			    printf(COLOR_CYAN "Client requested %d job(s).", numberOfJobs);
    			    printf(COLOR_RESET "\n");
                }
    			for(int i = 0; i < numberOfJobs; i++){
    				usleep(5000);
    				if(sendJob() == -1){
                        return;
    				}
    			}
    			break;
    		case 'U':
    			debugPrint("Client requested all jobs.", debug);
    			while(1){
    				usleep(2500);
    				if(sendJob() == -1){
    					return;
    				}
    			}
    			break;
    		case 'T':
    		    debugPrint("Client signaled termination.", debug);
    			return;
    		case 'E':
    		    debugPrint("Client signaled termination from an error.", debug);
                errorCode(((client_message>>8)&255));
    			return;
            case 'Q':
                debugPrint("Client shutdown caused by CTRL+C. SIGINT", debug);
                return;
    		default:
                printf("%d\n", client_message);
    		    debugPrint("Unfamiliar command. Shutting down server.", debug);
                sendTermSignal(3);
    			return;
    	}
	}
}

/* Reads a job from the jobfile
 * and puts together the information
 * so it sends it in the decided upon
 * format.
 *
 * 3 bit  - jobtype
 * 5 bit  - checksum
 * 4 char - a split integer
 *          containing text-length.
 * Rest   - the actual text
 *
 * Input:
 *     none
 * Return:
 *     -1 on error
 *      0 on success
 */
int sendJob(){
    char job_type;
    if(fread(&job_type, sizeof(char), 1, f) == 0){
        debugPrint("Out of jobs. Alerting client.", debug);
        outOfJobs();
        return -1;
    }

    unsigned int text_length;
    if(fread(&text_length, sizeof(int), 1, f) == 0){
        errorPrint("Error with reading text-length. Inspect job file.");
        sendTermSignal(2);
        return -1;
    }
    if(text_length < 1 || text_length > 1000000){
        errorPrint("Text-length out of bounds. Inspect job file.");
        return -1;
    }
    char job_text[text_length+1];
    memset(job_text,0, sizeof(job_text));
    job_text[text_length] = '\0';
    if(fread(&job_text, sizeof(char), text_length, f) == 0){
        errorPrint("Error with reading job-text. Inspect job file.");
        sendTermSignal(2);
        return -1;
    }

    int checksum = getChecksum(job_text, text_length);
    if(checksum < 0 || checksum > 32){
        errorPrint("Checksum value out of bounds.");
        sendTermSignal(2);
        return -1;
    }
    unsigned char job_info = checksum;

    switch (job_type){
        case 'O':
            break;
        case 'E':
            job_info = job_info + (1<<5);
            break;
        default:
            errorPrint("Invalid job type.");
            sendTermSignal(2);
            return -1;
    }

    char outMessage[text_length+2+sizeof(int)];
    memset(outMessage,0, sizeof(outMessage));
    outMessage[0] = job_info;
    for(int i = 0; i < (int)sizeof(int); i++){
        int shuffle = 12 -(i*4);
        outMessage[i+1] = (char)((text_length >> shuffle) & 15);
    }
    if(debug){
        printf(COLOR_CYAN">>%d<< Sending job of type %c\n", getpid(), job_type);
        printf(">>%d<< text length is: %d\n",getpid(), text_length);
        printf(">>%d<< checksum is:    %d\n",getpid(), checksum);
        printf(COLOR_YELLOW "\n%s\n", job_text);
        printf("\n"COLOR_RESET);
    }
    strcpy((outMessage+1+sizeof(int)), job_text);
    send(client_socket, outMessage, sizeof(outMessage), 0);
    return 0;
}

/* Reads and prints the reason
 * the client had to quit due
 * to an error
 * Error code protocol described
 * in protokoll.txt.
 *
 * Input:
 *     type: integer error code
 * Result:
 *     void
 */
void errorCode(int type){
    switch(type){
        case 'S':
            debugPrint("Client shutdown caused by checksum-error.", debug);
            break;
        case 'C':
            debugPrint("Client shutdown caused by error in a child process.", debug);
            break;
        default:
            debugPrint("Client shutdown caused by unkown error.", debug);
            break;
    }
}
/* Sends an otherwise empty message
 * containing only the signal for
 * termination of the server, where the
 * job-type is usually stated. So the
 * client is informed of the server terminating
 * and why.
 *
 * Input:
 *     sig: integer error code
 * Return:
 *     void
 */
void sendTermSignal(int sig){
    unsigned char jobempty[5] ={0};
    unsigned char empty= sig << 5;
    jobempty[0] = empty;
    send(client_socket, jobempty, 5*sizeof(char), 0);
}
/* Sends an otherwise empty message
 * containing only the signal for
 * termination of the server. So the
 * client is informed that it shuts down.
 * Then the server waits for client to
 * message back that it will also terminate.
 *
 * Input:
 *     none
 * Return:
 *     void
 */
void outOfJobs(){
    sendTermSignal(7);
    debugPrint("Awaiting termination confirmation from client...", debug);

    int client_message = 0;
    recv(client_socket, &client_message,sizeof(client_message), 0);
    int request = client_message&255;
    switch(request){
        case 'Q':
            debugPrint("Client shutdown caused by CTRL+C. SIGINT", debug);
            return;
        case 'T':
            debugPrint("Client termination message received.", debug);
            return;
        case 'E':
            debugPrint("Client termination was due to error.", debug);
            errorCode((client_message>>8)&255);
            return;
        default:
            break;
    }
}

/* Signal handler that makes sure
 * the server shuts down in a good
 * way when the user interrupts with
 * CTRL+C.
 * Tells client that this is the
 * reason server is shutting down.
 *
 * Input:
 *     sig: integer signal
 * Return:
 *    void
 */
void signalHandler(int sig){
    assert(sig == SIGINT);
    fclose(f);
	sendTermSignal(6);
    debugPrint("Shutting down server...", debug);
    close(client_socket);
    exit(EXIT_FAILURE);
}
