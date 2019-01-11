/* client.c
 ******************************************
 * USAGE:
 * Arguments: <hostname> <port> -DEBUG
 *
 *
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <netdb.h>

#include "colors.h"

/* Fields 		*/
int network_socket;
int pipe_child1[2];
int pipe_child2[2];
int pipe_parent[2];
int debug;

pid_t parentid;
pid_t child1_id;
pid_t child2_id;

/* Functions 	*/
void usage(int argc, char* argv[]);
void createPipes();
void childOneBehaviour();
void childTwoBehaviour();
void createSocket(char* address, char* port);
void userMenu();
int receiveJob();
int sendMessage(int message);
int checkServerTerm(char type);
void getIntput(int* input);
void killChildren();
void shutdownError(char* string, char type);
void parentSignalHandler(int sig);
void childSignalHandler(int sig);

void debugPrint(char *string, int debug);
void errorPrint(char *string);
int portCheck(char* port);
int getChecksum(char *string, int length);

/* Main-function that checks if the
 * program was supplied the right amount
 * of arguments, and if we are running
 * the client in debug mode.
 *
 * Sets up pipes for communication
 * between parent and children.
 * Then sets up 2 children.
 * Then attempts to connect to a server,
 * and sends/recieves data.
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
    parentid = getpid();
    usage(argc, argv);

    struct sigaction sigint;
    memset(&sigint, 0, sizeof(sigint));
    sigint.sa_handler = parentSignalHandler;
    sigaction(SIGINT, &sigint, NULL);

    struct sigaction sigquit;
    memset(&sigquit, 0, sizeof(sigquit));
    sigquit.sa_handler = childSignalHandler;
    sigaction(SIGQUIT, &sigquit, NULL);

    createPipes();

    debugPrint("Forking the first child-process.", debug);
    child1_id = fork();
    if(child1_id == -1){
        errorPrint("Error occurred when forking first child.");
        exit(EXIT_FAILURE);
    }
    if(child1_id == 0){
        childOneBehaviour();
    }else{
        debugPrint("Forking second child process.", debug);
        child2_id = fork();
        if(child2_id == -1){
            errorPrint("Error occurred when forking first child.");
            exit(EXIT_FAILURE);
        }
        if(child2_id == 0){
            childTwoBehaviour();
        }else{
            close(pipe_child1[0]);
            close(pipe_child2[0]);
            close(pipe_parent[1]);
            debugPrint("Creating network socket.", debug);
            debugPrint("Attempting to connect to the server.", debug);
            createSocket(argv[1], argv[2]);
            if(network_socket == -1){
                errorPrint("Couldn't connect to server at:");
                printf(COLOR_RED ">>%d<< Hostname: %s\n", getpid(), argv[1]);
                printf(COLOR_RED ">>%d<< Port:     %s", getpid(), argv[2]);
                printf(COLOR_RESET"\n");
                killChildren();
                printf("Exiting program...\n");
                close(pipe_child1[1]);
                close(pipe_child2[1]);
                close(pipe_parent[0]);
                exit(EXIT_FAILURE);
            }
            userMenu();

            printf("Exiting program...\n");
            close(pipe_child1[1]);
            close(pipe_child2[1]);
            close(pipe_parent[0]);
            close(network_socket);
            return 0;
        }
    }
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
    debug = 0;
    if(argc < 3){
        errorPrint("Not enough program arguments supplied.");
        errorPrint("./client <hostname> <Port>");
        errorPrint("To run client in debug mode add last argument '-DEBUG'\n");
        exit(EXIT_FAILURE);
    }
    if(portCheck(argv[2]) == -1){
        errorPrint("Please choose a port from 1 to 6535.");
        errorPrint("./client <hostname> <Port>");
        errorPrint("To run client in debug mode add last argument '-DEBUG'\n");
        exit(EXIT_FAILURE);
    }
    if(argc == 4){
        char comp[7]= "-DEBUG";
        comp[6] = '\0';
        if(strcmp(comp, argv[3]) == 0){
            debug = 1;
            debugPrint("Running client in debug mode.", debug);
        }else{
            errorPrint("./client <hostname> <Port>");
            errorPrint("To run client in debug mode add last argument '-DEBUG'\n");
            exit(EXIT_FAILURE);
        }
    }
}

/* Sets up two pipes to communicate
 * with, and forks two child processes.
 * Also one pipe so the parent can wait
 * for the children to finish their printing.
 * This is to avoid overly bad sync issues
 * with the printing, and error handling
 * of the children. As they can use the
 * pipe to inform parent of errors.
 *
 * Input:
 *     none
 * Return:
 *     void
 *
 */
void createPipes(){
    debugPrint("Setting up pipes.", debug);
    if(pipe(pipe_child1)  == -1){
        errorPrint("Error with setting up pipes.");
        exit(EXIT_FAILURE);
    }
    if(pipe(pipe_child2)  == -1){
        errorPrint("Error with setting up pipes.");
        exit(EXIT_FAILURE);
    }
    if(pipe(pipe_parent)  == -1){
        errorPrint("Error with setting up pipes.");
        exit(EXIT_FAILURE);
    }
}

/* Function containing the
 * behaviour of the first
 * child.
 *
 * It reads job messages from parent
 * returning 'C' if successful and
 * 'E' if an error occurred.
 *
 * Input:
 *     none
 * Return:
 *    void
 */
void childOneBehaviour(){
    close(pipe_child1[1]);
    close(pipe_child2[1]);
    close(pipe_parent[0]);
    char err = 'E';
    debugPrint("Listening for messages from parent.", debug);
    while(1){
        char ch;
        if(read(pipe_child1[0], &ch, sizeof(char)) == 0){
            write(pipe_parent[1], &err, sizeof(char));
            continue;
        }
        if (ch == 'Q') {
            break;
        }
        if (ch != 'O') {
          write(pipe_parent[1], &err, sizeof(char));
          continue;
        }
        int text_length = 0;
        for(int i = 0; i < (int)sizeof(int); i++){
            int shuffle = 12 -(i*4);
            char ch;
            if(read(pipe_child1[0], &ch, sizeof(char)) == -1){
                write(pipe_parent[1], &err, sizeof(char));
                continue;
            }
            text_length = text_length +(ch << shuffle);
        }
        printf("\n");
        debugPrint("Received job from parent.", debug);
        if(debug ==1){
            printf(COLOR_CYAN">>%d<< text length: %d",getpid(), text_length);
            printf(COLOR_RESET"\n");
        }
        char print_text[text_length+1];
        print_text[text_length] = '\0';
        if(read(pipe_child1[0], &print_text, text_length*sizeof(char))==-1){
                write(pipe_parent[1], &err, sizeof(char));
                continue;
        }
        fprintf(stdout,COLOR_YELLOW "%s", print_text);
        printf("\n"COLOR_RESET);

        char cont = 'C';
        write(pipe_parent[1],&cont,sizeof(char));
        fflush(stdout);
    }
    debugPrint("Terminating child process #1...", debug);
    close(pipe_child1[0]);
    close(pipe_child2[0]);
    close(pipe_parent[1]);
    exit(EXIT_SUCCESS);
}

/* Function containing the
 * behaviour of the
 * second child.
 *
 * It reads job messages from parent
 * returning 'C' if successful and
 * 'E' if an error occurred.
 *
 * Input:
 *     none
 * Return:
 *    void
 */
void childTwoBehaviour(){
    close(pipe_child1[1]);
    close(pipe_child2[1]);
    close(pipe_parent[0]);
    char err = 'E';
    debugPrint("Listening for messages from parent.", debug);
    while(1){
        char ch;
        if(read(pipe_child2[0], &ch, sizeof(ch))==-1){
            write(pipe_parent[1], &err, sizeof(char));
            continue;
        }
        if (ch == 'Q') {
            break;
        }
        if (ch != 'E') {
          write(pipe_parent[1], &err, sizeof(char));
          continue;
        }
        int text_length = 0;
        for(int i = 0; i < (int)sizeof(int); i++){
            int shuffle = 12 -(i*4);
            char ch;
            if(read(pipe_child2[0], &ch, sizeof(char))==-1){
                write(pipe_parent[1], &err, sizeof(char));
                continue;
            }
            text_length = text_length +(ch << shuffle);
        }
        printf("\n");
        debugPrint("Received job from parent.", debug);
        if(debug == 1){
            printf(COLOR_CYAN">>%d<< text length: %d",getpid(), text_length);
            printf(COLOR_RESET"\n");
        }
        char print_text[text_length+1];
        print_text[text_length] = '\0';
        read(pipe_child2[0], &print_text, text_length);
        fprintf(stdout,COLOR_MAGENTA "%s", print_text);
        printf("\n"COLOR_RESET);

        char cont = 'C';
        write(pipe_parent[1],&cont,sizeof(char));
        fflush(stderr);
    }
    debugPrint("Terminating child process #2...", debug);
    close(pipe_child2[0]);
    close(pipe_child1[0]);
    close(pipe_parent[1]);
    exit(EXIT_SUCCESS);
}

/* Creates a networking socket and
 * attempts to connect to the server
 * through the hostname and port
 * specified by the user.
 *
 * Input:
 *     hostname: char* hostname
 *     port:     char* port
 * Return:
 *     void
 */
void createSocket(char* hostname, char* port){
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostname, port, &hints, &result) != 0){
        network_socket = -1;
        return;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        network_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (network_socket == -1) {
            continue;
        }
        if (connect(network_socket, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }
    }
    if (rp == NULL) {
        errorPrint("Could not connect.\n");
        freeaddrinfo(result);
        network_socket = -1;
        return;
    }

    char ipstring[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &((struct sockaddr_in *)rp->ai_addr)->sin_addr, ipstring, sizeof ipstring);
    printf(COLOR_CYAN">>%d<<Connected to server at: %s",getpid(), ipstring);
    printf(COLOR_RESET"\n");

    freeaddrinfo(result);
}

/* Interactive terminal menu that lets the user
 * send 4-byte messages to the server.
 * Exits the command-loop when the
 * server runs out of jobs, if the server
 * signals an error, or if the client itself
 * experiences an error.
 *
 * After sending a job to a child, the parent
 * waits for a char-message that signals
 * that the child is done printing.
 * 'C' if the child was succesful, and
 * 'E' if it experienced an error.
 * This is only checked with ret != 'C'
 * however; as any other return value
 * would also be considered an error.
 *
 * Input:
 *    none
 * Return:
 *     void
 *
 */
void userMenu(){
    int choice = 0;
    int request;

    while(1){
        usleep(2500);
    	printf(COLOR_RESET"\n-------Server-connected client-------");
    	printf(COLOR_RESET"\n[1] Get job from server.");
    	printf(COLOR_RESET"\n[2] Get multiple jobs from server.");
    	printf(COLOR_RESET"\n[3] Get all jobs from server.");
    	printf(COLOR_RESET"\n[4] Exit program.\n");
    	getIntput(&choice);

    	if(choice == 1){
    		request = 'J';
            request = request + (1 << 8);
            if(sendMessage(request) == -1){
                return;
            }
    		if(receiveJob() == -1){
                return;
            }
            char ret;
            read(pipe_parent[0], &ret, sizeof(char));
            if(ret != 'C'){
                shutdownError("Error ocurred in child.", 'C');
                return;
            }
            debugPrint("Child done working. Resuming parent.",debug);

    	}
    	if(choice == 2){
            printf(COLOR_RESET"How many jobs to request?\n");
            int howmany;
            getIntput(&howmany);
    		while(howmany > 8388607 || howmany < 1){
                errorPrint("Amount out of bounds.");
                printf("Request jobs between 1 and 8388607.\n");
                printf(COLOR_RESET"How many jobs to request?\n");
                getIntput(&howmany);
            }

            request = 'J';
            request = request + (howmany << 8);

            if(sendMessage(request) == -1){
                return;
            }
            for(int i = 0; i < howmany; i++){
                if(receiveJob() == -1){
                    return;
                }
                char ret;
                read(pipe_parent[0], &ret, sizeof(char));
                if(ret != 'C'){
                    shutdownError("Error ocurred in child.", 'C');
                    return;
                }
                debugPrint("Child done working. Resuming parent.", debug);
            }
    	}
    	if(choice == 3){
            request ='U';
            if(sendMessage(request) == -1){
                return;
            }
            while(1){
                if(receiveJob() == -1){
                    return;
                }
                char ret;
                read(pipe_parent[0], &ret, sizeof(char));
                if(ret != 'C'){
                    shutdownError("Error ocurred in child.", 'C');
                    return;
                }
                debugPrint("Child done working. Resuming parent.", debug);
            }
        }
    	if(choice == 4){
            killChildren();
            request= 'T';
            if(sendMessage(request) == -1){
                return;
            }
            break;
    	}
        if(choice > 4 || choice < 1){
            printf("Not a valid choice. Try again.\n");
        }
    	printf("\n\n");
    }
}

/* Attempts to send the message from the
 * input to the server.
 *
 * Input:
 *     message: Integer message for server
 * Return:
 *    0 on success.
 *   -1 on error.
 *
 */
int sendMessage(int message){
    if(debug == 1){
        printf(COLOR_CYAN ">>%d<< Sending message to server: ", getpid());
        printf("%c \n"COLOR_RESET,(char)(message&255));
    }
    if(send(network_socket, &message, sizeof(message), 0) == -1){
        errorPrint("Error while attempting to message server!");
        return -1;
    }
    return 0;
}

/* Behaviour for receiving the job
 * from the server. Picking apart
 * the message and sending it to
 * the correct child.
 * If the server sends a termination message
 * it makes sure to kill the children, and
 * terminate correctly.
 *
 * Input:
 *     none
 * Return:
 *    0 on success.
 *   -1 on error.
 */
int receiveJob(){
    unsigned char job_info;
    if(recv(network_socket, &job_info, sizeof(char), 0) == -1){
        errorPrint("Lost connection to server.");
        debugPrint("Terminating children...", debug);
        killChildren();
        return-1;
    }
    unsigned char job_type = job_info >> 5;
    if(checkServerTerm(job_type) == -1){
        return -1;
    }
    char lengthchars[4];

    char checksum = job_info & 31;
    int text_length = 0;
     for(int i = 0; i < (int)sizeof(int); i++){
        int shuffle = 12 -(i*4);
        char ch;
        if(recv(network_socket, &ch, sizeof(char), 0) == -1){
            errorPrint("Lost connection to server.");
            killChildren();
            return-1;
        }
        lengthchars[i] = ch;
        text_length = text_length +(ch << shuffle);
    }
    char temp_text[text_length+1];
    if(recv(network_socket, &temp_text, sizeof(temp_text), 0) == -1){
        errorPrint("Lost connection to server.");
        killChildren();
        return-1;
    }
    if(checksum != getChecksum(temp_text, text_length)){
        errorPrint("Error! Checksum did not match.");
        shutdownError("Terminating client due to checksum error.", 'S');
        return -1;
    }
    char job_text[text_length+5];
    memset(job_text,0, text_length+5);
    for(int i = 0; i < 4; i++){
        job_text[i+1] = lengthchars[i];
    }
    for(int i = 0; i < text_length; i++){
        job_text[i+5] = temp_text[i];
    }

    if(debug == 1){
        printf(COLOR_CYAN"\n>>%d<< Received job from server:", getpid());
        printf(COLOR_CYAN"\n>>%d<< jobtype:    %d", getpid(), job_type);
        printf(COLOR_CYAN"\n>>%d<< textlength: %d", getpid(), text_length);
        printf(COLOR_CYAN"\n>>%d<< checksum:   %d", getpid(), checksum);
        printf(COLOR_RESET"\n");
    }
    switch(job_type){
        case 0:
            job_text[0] = 'O';
            write(pipe_child1[1], job_text, sizeof(job_text));
            break;
        case 1:
            job_text[0] = 'E';
            write(pipe_child2[1], job_text, sizeof(job_text));
            break;
    }
    return 0;
}

/* Function that checks for
 * termination messages from
 * the server in the job_type
 * part of the job message.
 * protocol described in protokoll.txt
 *
 * Input:
 *     type: char message
 * Return:
 *     0 if server still alive
 *    -1 if termination message
 *     received
 */
int checkServerTerm(char type){
    switch (type){
        case 7:
            debugPrint("Server out of Jobs.", debug);
            int terrMsg = 'T';
            sendMessage(terrMsg);
            usleep(250000);
            killChildren();
            return -1;
        case 2:
            debugPrint("Server terminated due to filereading error.", debug);
            killChildren();
            return -1;
        case 3:
            debugPrint("Server terminated due to receiving unkown request.", debug);
            killChildren();
            return -1;
        case 6:
            debugPrint("Server terminated due to CTRL+C (sigint).", debug);
            killChildren();
            return -1;
    }
    return 0;
}

/* Prompts user for an integer
 * and saves the integer in the
 * pointer.
 *
 * Input:
 *     input: integer pointer
 * Return:
 *     void
 *
 */
void getIntput(int* input){
    char *p, s[100];
    while (fgets(s, sizeof(s), stdin)) {
        *input = strtol(s, &p, 10);
        if (p == s || *p != '\n') {
            printf("Not an integer. Try Again\n");
        }else break;
    }
}

/* Kills the children by sending both
 * of them termination messages.
 *
 * Input:
 *     none
 * Returns:
 *     void
 */
void killChildren(){
    debugPrint("Terminating children...", debug);
    char jobempty[2] = {0};
    jobempty[0] = 'Q';
    write(pipe_child1[1], &jobempty, 2*sizeof(char));
    write(pipe_child2[1], &jobempty, 2*sizeof(char));
}

/* When an error occurs this method
 * prints the reason from "string".
 * Then it kills the children and
 * alerts the server of client
 * termination, of the type stated.
 *
 * Input:
 *     string: char* with error msg
 *     type: char describing error type
 *
 * Return:
 *    void
 *
 */
void shutdownError(char* string, char type){
    errorPrint(string);
    killChildren();
    debugPrint("Sending termination message to server.", debug);
    int request = 'E';
    request = request + (type << 8);
    sendMessage(request);
}

/* Signal handler that makes sure
 * the children shut down in a good
 * way when the user interrupts with
 * CTRL+C.
 *
 * Input:
 *     sig: integer signal
 * Return:
 *    void
 */
void childSignalHandler(int sig){
    assert(sig == SIGQUIT);
    pid_t self = getpid();
    close(pipe_child1[0]);
    close(pipe_child2[0]);
    close(pipe_parent[1]);
    if (parentid != self) {
        debugPrint("Child process terminating...", debug);
        exit(EXIT_SUCCESS);
    }
}

/* Signal handler that makes sure
 * the parent shuts down in a good
 * way when the user interrupts with
 * CTRL+C.
 *
 * Input:
 *     sig: integer signal
 * Return:
 *    void
 */
void parentSignalHandler(int sig) {
    assert(sig == SIGINT);

    int request = 'Q';
    send(network_socket, &request, sizeof(int), 0);

    close(pipe_child1[1]);
    close(pipe_child2[1]);
    close(pipe_parent[0]);
    kill(parentid, SIGQUIT);
    usleep(2000);
    close(network_socket);
    exit(EXIT_FAILURE);
}
