#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include "utils.h"

#define BUFFER_SIZE 256
#define GAME_INFO_HEADER 0xDEADFACE

// to store messages and to display them properly
char all_messages[1024][BUFFER_SIZE];
int message_count = 0;

// game stuff
game_info gameInfo;

void* receive_messages(void* socket);

int main(int argc, char *argv[]) {
    int mySocket = 0, clientSocket = 0;
    int port = 0;
    int returnStatus = 0;
    struct sockaddr_in hangmanServer, clientName;
    pthread_t receiverThread;

    pthread_mutex_t messages_mutex = PTHREAD_MUTEX_INITIALIZER;

    // game stuff initialize
    gameInfo.guessedCounter = 0;
    gameInfo.gameState = 0;
    gameInfo.lives = 8;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    mySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mySocket == -1) {
        printf("Could not create a socket!\n");
        exit(1);
    } else {
        printf("Socket created!\n"); //             remove later
    }

    port = atoi(argv[1]);   // port number for listening

    // Set up address structure
    bzero(&hangmanServer, sizeof(hangmanServer));
    hangmanServer.sin_family = AF_INET;
    hangmanServer.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY to bind to all local addresses
    hangmanServer.sin_port = htons(port);

    returnStatus = bind(mySocket, (struct sockaddr *)&hangmanServer, sizeof(hangmanServer));

    if (returnStatus == 0) {
        printf("Bind completed!\n");
    } else {
        printf("Couldn't bind to address!\n");
        close(mySocket);
        exit(1);
    }

    returnStatus = listen(mySocket, 5); // 5 is arbitrary, maybe >2 players later
    if (returnStatus == -1) {
        printf("Cannot listen on socket!\n");
        close(mySocket);
        exit(1);
    }

    printf("Waiting for Player2 to connect...\n");

    socklen_t clientLen = sizeof(clientName);
    clientSocket = accept(mySocket, (struct sockaddr *)&clientName, &clientLen);
    if (clientSocket == -1) {
        perror("Accept failed");
        close(mySocket);
        exit(1);
    }
    printf("Player2 connected!\n");


    thread_args args;
    args.socket = clientSocket; // or serverSocket
    args.gameInfo = &gameInfo;  // Pass the pointer to gameInfo
    args.all_messages = all_messages;
    args.message_count = &message_count;
    // create new thread to accept messages
    returnStatus = pthread_create(&receiverThread, NULL, receive_messages_common, (void*)&args);
    if (returnStatus != 0) {
        printf("Couldn't create message thread!");
        close(mySocket);
        exit(1);
    }
    // Send messages
    char buffer[BUFFER_SIZE];
    while(1) {
        printf("You: ");
        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);

        buffer[strcspn(buffer, "\n")] = '\0'; // replace new-line with null-terminate

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
            write(clientSocket, buffer, strlen(buffer));
        }

        if (strncmp(buffer, "quit", 4) == 0) {
            printf("\nClosing connection.\n");
            close(clientSocket);
            break;
        }
    }
    pthread_cancel(receiverThread);
    pthread_join(receiverThread, NULL);
    close(mySocket);
    return 0;
}
