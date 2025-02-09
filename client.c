#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "utils.h"

#define BUFFER_SIZE 256
#define GAME_INFO_HEADER 0xDEADFACE

// to store and then print messages properly
char all_messages[1024][BUFFER_SIZE];
int message_count = 0;

game_info gameInfo;

void* receive_messages(void* socket);

int main(int argc, char* argv[]) {
    int mySocket = 0;
    int port = 0;
    int returnStatus = 0;
    struct sockaddr_in hangmanServer;
    pthread_t receiverThread;

    pthread_mutex_t messages_mutex = PTHREAD_MUTEX_INITIALIZER;

    // game stuff initialize
    gameInfo.guessedCounter = 0;
    gameInfo.gameState = 0;
    gameInfo.lives = 8;

    if (argc != 3) {
        printf("Usage: %s <server> <port>\n", argv[0]);
        exit(1);
    }

    mySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mySocket == -1) {
        printf("Couldn't create socket!\n");
    } else {
        printf("Socket created!\n");
    }

    port = atoi(argv[2]); // retrieve port to connect

    // set up address struct
    bzero(&hangmanServer, sizeof(hangmanServer));
    hangmanServer.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, argv[1], &hangmanServer.sin_addr) <= 0) {
        printf("Invalid address or address not supported\n");
        exit(1);
    }

    hangmanServer.sin_port = htons(port); 

    // connect to server
    returnStatus = connect(mySocket, (struct sockaddr *)&hangmanServer, sizeof(hangmanServer));
    if (returnStatus == 0) {
        printf("Succesfully connected to server!\n");
    } else {
        printf("Couldn't connect to server!\n");
        close(mySocket);
        exit(1);
    }

    thread_args args;
    args.socket = mySocket; // or serverSocket
    args.gameInfo = &gameInfo;  // Pass the pointer to gameInfo
    args.all_messages = all_messages;
    args.message_count = &message_count;

    // Create a thread to receive messages
    returnStatus = pthread_create(&receiverThread, NULL, receive_messages_common, (void*)&args);
    if (returnStatus != 0) {
        printf("Couldn't create message thread!");
        close(mySocket);
        exit(1);
    }

    // Message sending loop
    char buffer[BUFFER_SIZE];
    while (1) {
        printf("You: ");
        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);

        buffer[strcspn(buffer, "\n")] = '\0'; // remove newline char

        // Store the message
        int msglen = strlen(buffer);
        char storedMessage[msglen + 6];
        snprintf(storedMessage, sizeof(storedMessage), "You: %s", buffer);

        pthread_mutex_lock(&messages_mutex);
        strncpy(all_messages[message_count], storedMessage, BUFFER_SIZE);
        message_count++;
        pthread_mutex_unlock(&messages_mutex);

        catchSentCommand(buffer, &gameInfo); // updtes game info according to command

        if (strlen(buffer) > 0) { // only send non-empty messages
            write(mySocket, buffer, strlen(buffer));
        }
        if (strncmp(buffer, "quit", 4) == 0) {
            printf("Closing connection.\n");
            close(mySocket);
            break;
        }
    }
    pthread_cancel(receiverThread);
    pthread_join(receiverThread, NULL);
    close(mySocket);
    return 0;
}
