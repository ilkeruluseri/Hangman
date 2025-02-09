#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

#define BUFFER_SIZE 256

/*returns 0 if word contains letter, 1 if not*/
int contains(char* word, char letter) {
    for (int i = 0; i < strlen(word); i++) {
        if (letter == word[i]) {
            return 0;
        }
    }
    return 1;
}

/*Replaces undersocres with guessed letters in the word.*/
char* generateHiddenWord(char* word, char* guessedLetters) {
    int wordLen = strlen(word);
    char* hiddenword = malloc(wordLen * 2 + 1);
    if (!hiddenword) {
        printf("Failed to allocate memory");
        exit(1);
    }
    bzero(hiddenword, sizeof(hiddenword));

    for (int i = 0; i < wordLen * 2; i ++) {
        if (i % 2 == 1) {
            hiddenword[i] = ' ';
        } else {
            if (contains(guessedLetters, word[i/2]) == 0) { // letter has been guessed
                hiddenword[i] = word[i/2];
            } else {
                hiddenword[i] = '_';
            }
        }
    }
    hiddenword[wordLen * 2] = '\0';
    return hiddenword;
}

/*Formats guessed letters with commas*/
char* generateGuessedLetters(char* guessedLetters) {
    int numOfLetters = strlen(guessedLetters);
    char* newString = malloc(numOfLetters * 3 + 1);
    for (int i = 0; i < strlen(guessedLetters); i++) {
        newString[i * 3] = guessedLetters[i];
        newString[i * 3 + 1] = ',';
        newString[i * 3 + 2] = ' ';
    }
    newString[numOfLetters * 3 - 2] = '\0'; // last comma turns to null-terminate

    return newString;
}

/*Checks to see if word is comprised entirley of letters
Returns 0 if yes, 1 if no*/
int comprised(char* word, char* letters) {
    for (int i = 0; i < strlen(word); i++) {
        if(contains(letters, word[i]) == 1) { // if ith letter in word not in letters
            return 1;
        }
    }
    return 0;
}

typedef struct {
    char chosenWord[64];
    char guessedLetters[26];
    int guessedCounter;
    int gameState; // 0- word not set, 1- word has been set
    int lives;
} game_info;

typedef struct {
    int socket;
    game_info* gameInfo;
    char (*all_messages)[BUFFER_SIZE];
    int *message_count;
} thread_args;


void resetGameInfo(game_info* info) {
    bzero(info->chosenWord, 64);
    bzero(info->guessedLetters, 26);
    info->gameState = 0;
    info->lives = 8;
    info->guessedCounter = 0;
}

void *receive_messages_common(void *args) {

    thread_args *params = (thread_args *)args;
    int mySocket = params->socket;
    game_info *gameInfo = params->gameInfo;
    char (*all_messages)[BUFFER_SIZE] = params->all_messages;
    int *message_count  = params->message_count;

    pthread_mutex_t messages_mutex = PTHREAD_MUTEX_INITIALIZER;

    char buffer[BUFFER_SIZE];

    while (1) {
        bzero(buffer, sizeof(buffer));
        int bytesRead = read(mySocket, buffer, sizeof(buffer) - 1);

        if (bytesRead > 0) {
            buffer[bytesRead] = '\0'; // null-terminate

            // HANGMAN COMMANDS
            if (strncmp(buffer, "WORD ", 5) == 0) {             // SET WORD
                if (gameInfo->gameState == 1) {
                    strcpy(buffer, "GAME: Word has already been set! Use RESTART to restart the game.");
                    write(mySocket, buffer, BUFFER_SIZE);

                    // Store the message
                    pthread_mutex_lock(&messages_mutex);
                    strncpy(all_messages[*message_count], buffer, BUFFER_SIZE);
                    (*message_count)++;
                    pthread_mutex_unlock(&messages_mutex);
                }
                else {
                    resetGameInfo(gameInfo); // reset game info just in case
                    const char* word = buffer + 5; // Skip "WORD " to get the rest
                    if (strlen(word) > 0 && strchr(word, ' ') == NULL) {
                        strcpy(gameInfo->chosenWord, word);
                        gameInfo->gameState = 1;
                        char hiddenWord[strlen(gameInfo->chosenWord)*2 + 1];
                        bzero(hiddenWord, sizeof(hiddenWord));
                        for (int i = 0; i < strlen(gameInfo->chosenWord)*2; i+=2) {
                            hiddenWord[i] = '_';
                            hiddenWord[i+1] = ' ';
                        }
                        hiddenWord[strlen(hiddenWord)] = '\0';
                        strcpy(buffer, "GAME: Word has been set! Word: ");
                        strcat(buffer, hiddenWord);
                        char livesString[10];
                        sprintf(livesString, "Lives: %d", gameInfo->lives);
                        strcat(buffer, livesString);
                        write(mySocket, buffer, BUFFER_SIZE);

                        // Store the message
                        pthread_mutex_lock(&messages_mutex);
                        strncpy(all_messages[*message_count], buffer, BUFFER_SIZE);
                        (*message_count)++;
                        pthread_mutex_unlock(&messages_mutex);
                    } else {
                        strcpy(buffer, "GAME: Invalid WORD command. Use: WORD <word> (word shouldn't have spaces)\n");
                        write(mySocket, buffer, BUFFER_SIZE);

                        // Store the message
                        pthread_mutex_lock(&messages_mutex);
                        strncpy(all_messages[*message_count], buffer, BUFFER_SIZE);
                        (*message_count)++;
                        pthread_mutex_unlock(&messages_mutex);
                    }
                }
                
            } else if (strncmp(buffer, "GUESS ", 6) == 0 && gameInfo->gameState == 1) {     // GUESS LETTER
                // Store guess command from other player no matter what
                char guessCommand[BUFFER_SIZE + 16];
                snprintf(guessCommand, BUFFER_SIZE + 16, "Other Player: %s", buffer);
                pthread_mutex_lock(&messages_mutex);
                strncpy(all_messages[*message_count], guessCommand, BUFFER_SIZE);
                (*message_count)++;
                pthread_mutex_unlock(&messages_mutex);

                const char* letter = buffer + 6;
                if (strlen(letter) == 1 && isalpha(letter[0])) {
                    if (contains(gameInfo->guessedLetters, letter[0]) == 0) {
                        strcpy(buffer, "GAME: Letter already guessed!");
                        write(mySocket, buffer, BUFFER_SIZE);

                        // Store the message
                        pthread_mutex_lock(&messages_mutex);
                        strncpy(all_messages[*message_count], buffer, BUFFER_SIZE);
                        (*message_count)++;
                        pthread_mutex_unlock(&messages_mutex);
                    }
                    else {
                        gameInfo->guessedLetters[gameInfo->guessedCounter] = letter[0]; // add to guessed list
                        gameInfo->guessedCounter++;
                        char* hiddenWord = generateHiddenWord(gameInfo->chosenWord, gameInfo->guessedLetters);
                        char* guessedLettersString = generateGuessedLetters(gameInfo->guessedLetters);

                        if (contains(gameInfo->chosenWord, letter[0]) == 0) { // guessed letter in word
                            if (comprised(gameInfo->chosenWord, gameInfo->guessedLetters) == 0) { // word found
                                snprintf(buffer, BUFFER_SIZE, "GAME: You did it :D\nThe word was: %s", gameInfo->chosenWord);
                                write(mySocket, buffer, BUFFER_SIZE);

                                // end and reset game 
                                resetGameInfo(gameInfo);
                            } else { // word not yet found
                                snprintf(buffer, BUFFER_SIZE, "GAME: %c is in the word! Word: %s  Lives: %i\nGuessed Letters: %s",
                                    letter[0], hiddenWord, gameInfo->lives, guessedLettersString);
                                write(mySocket, buffer, BUFFER_SIZE);
                            }
                            // Store the message
                            pthread_mutex_lock(&messages_mutex);
                            strncpy(all_messages[*message_count], buffer, BUFFER_SIZE);
                            (*message_count)++;
                            pthread_mutex_unlock(&messages_mutex);
                        } else { // guessed letter not in word
                            gameInfo->lives--;
                            if (gameInfo->lives <= 0) {
                                snprintf(buffer, BUFFER_SIZE, "GAME: You ran out of lives, so you lose :( The word was: %s", gameInfo->chosenWord);
                            }
                            else {
                                snprintf(buffer, BUFFER_SIZE, "GAME: %c is not in the word :( Word: %s  Lives: %i\nGuessed Letters: %s",
                                    letter[0], hiddenWord, gameInfo->lives, guessedLettersString);
                            }
                            write(mySocket, buffer, BUFFER_SIZE);

                            // Store the message
                            pthread_mutex_lock(&messages_mutex);
                            strncpy(all_messages[*message_count], buffer, BUFFER_SIZE);
                            (*message_count)++;
                            pthread_mutex_unlock(&messages_mutex);

                            if (gameInfo->lives <= 0) {
                                // End and Reset game here
                                resetGameInfo(gameInfo);
                            }
                        }
                        free(guessedLettersString);
                        free(hiddenWord);
                    }
                    
                } else {
                    strcpy(buffer, "GAME: Invalid GUESS command. Use: GUESS <letter>\n");
                    write(mySocket, buffer, BUFFER_SIZE);
                }
            } else if (strncmp(buffer, "GUESS ", 6) == 0 && gameInfo->gameState == 0){
                snprintf(buffer, BUFFER_SIZE, "GAME: Game not started!");
                write(mySocket, buffer, BUFFER_SIZE);
            } else if (strncmp(buffer, "STATUS", 6) == 0) {
                if (gameInfo->gameState == 1) {
                    snprintf(buffer, BUFFER_SIZE, "GAME:\n-----CURRENT GAME STATUS-----\nWord: %s, Lives: %i\nGuessed Letters: %s\n-----------------------------",
                    generateHiddenWord(gameInfo->chosenWord, gameInfo->guessedLetters),
                    gameInfo->lives, generateGuessedLetters(gameInfo->guessedLetters));
                } else {
                    snprintf(buffer, BUFFER_SIZE, "GAME: Game not started!");
                }
                // send this to other player, no need to print it here
                write(mySocket, buffer, BUFFER_SIZE);

            } else if (strncmp(buffer, "RESTART", 7) == 0) {
                if (gameInfo->gameState == 1) {
                    snprintf(buffer, BUFFER_SIZE, "GAME: Restarting game");
                    resetGameInfo(gameInfo); // gameInfo is a pointer
                }
                else {
                    snprintf(buffer, BUFFER_SIZE, "GAME: Game not started!");
                }
                // Store the message
                pthread_mutex_lock(&messages_mutex);
                strncpy(all_messages[*message_count], buffer, BUFFER_SIZE);
                (*message_count)++;
                pthread_mutex_unlock(&messages_mutex);
                // Send it as well
                write(mySocket, buffer, BUFFER_SIZE);
                
            } else if (strncmp(buffer, "HELP", 4) == 0) {
                snprintf(buffer, BUFFER_SIZE, "GAME:\nCommands:\nWORD - sets word\nGUESS - guess to see if a letter is in word\nSTATUS - see current game status\nRESTART - restart game, resets everything\nHELP - prints this message\nQUIT - quit program");
                write(mySocket, buffer, BUFFER_SIZE); // Only send to player who requested
            }
            else { // If no command, it must be a message

                if (strncmp(buffer, "GAME:", 5) == 0) { // GAME message instead of player
                    pthread_mutex_lock(&messages_mutex);
                    strncpy(all_messages[*message_count], buffer, BUFFER_SIZE);
                    (*message_count)++;
                    pthread_mutex_unlock(&messages_mutex);
                }
                else if (strlen(buffer) > 0){ // Don't print anything if buffer is empty
                    int msglen = strlen(buffer);
                    char storedMessage[msglen + 16];
                    snprintf(storedMessage, sizeof(storedMessage), "Other Player: %s", buffer);
                    pthread_mutex_lock(&messages_mutex);
                    strncpy(all_messages[*message_count], storedMessage, BUFFER_SIZE);
                    (*message_count)++;
                    pthread_mutex_unlock(&messages_mutex);
                }
            }

            /* // FOR DEBUG, print game info after receiving something ----------------------
            sprintf(buffer, "GAME INFO\nChosen Word: %s, Guessed Letters: %s, Lives: %d\n",
                    gameInfo->chosenWord, gameInfo->guessedLetters, gameInfo->lives);
            pthread_mutex_lock(&messages_mutex);
            strncpy(all_messages[(*message_count)], buffer, BUFFER_SIZE);
            (*message_count)++;
            pthread_mutex_unlock(&messages_mutex);
            */
            printf("\033[H\033[J"); // Clear screen
            //printf("DEBUG: Redrawing messages. Total messages: %d\n", (*message_count));
            printf("------------------HANGMAN------------------\n");
            for (int i = 0; i < (*message_count); i++) { // Reprint all messages
                printf("%s\n", all_messages[i]);
            }

            // Redraw the input prompt
            printf("You: ");
            fflush(stdout);

            if (strncasecmp(buffer, "quit", 4) == 0) { 
                printf("Other player disconnected.\n");
                close(mySocket);
                pthread_exit(NULL);
            }
        } 
        else if (bytesRead == 0) {
            printf("\nOther player closed the connection.\n");
            close(mySocket);
            pthread_exit(NULL);
        } 
        else {
            printf("\nError reading from other player\n");
            close(mySocket);
            pthread_exit(NULL);
        }
        
    }
}
/*Updates game info according to command.*/
void catchSentCommand(char* msg, game_info* info) {
    if (strncmp(msg, "WORD ", 5) == 0 && info->gameState == 0) {
        resetGameInfo(info); // reset game info just in case
        const char *word = msg + 5; // Skip WORD to get the rest

        if (strlen(word) > 0 && strchr(word, ' ') == NULL) {
            strcpy(info->chosenWord, word); // set chosen word
            info->gameState = 1; // update game state
        }
        
    } else if (strncmp(msg, "GUESS ", 6) == 0 && info->gameState == 1) {
        const char* letter = msg + 6;
        if (strlen(letter) == 1 && isalpha(letter[0]) && contains(info->guessedLetters, letter[0]) == 1) {
            info->guessedLetters[info->guessedCounter] = letter[0];
            info->guessedCounter++;
            if (contains(info->chosenWord, letter[0]) == 1) { // if not in word
                info->lives--;
                if (info->lives <= 0) { // game lost, reset
                    resetGameInfo(info);
                }
            } 
            else if (comprised(info->chosenWord, info->guessedLetters) == 0) { // game won, reset
                resetGameInfo(info);
            }
        }
    } else if (strncmp(msg, "RESTART", 7) == 0 && info->gameState == 1) {
        resetGameInfo(info);
    } else if (strncmp(msg, "STATUS", 6)) {

    }
}

