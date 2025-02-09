#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char* myWord = "hello";
    char guessedLetters[4];
    guessedLetters[0] = 'a';
    guessedLetters[1] = 'h';
    guessedLetters[2] = 'i';
    guessedLetters[3] = 'l';

    char* hiddenWord  =  generateHiddenWord(myWord, guessedLetters);

    printf("Word: %s\n", hiddenWord);
    free(hiddenWord);
    

    char* guessedLettersString = generateGuessedLetters(guessedLetters);
    printf("Guessed Letters: %s\n", guessedLettersString);
    free(guessedLettersString); 

    char* myWord2 = "hail";
    printf("\nTest1 w hello: %i\nTest2 w hail: %i\nExpected: 1 0\n", comprised(myWord, guessedLetters), comprised(myWord2, guessedLetters));


    return 0;
}