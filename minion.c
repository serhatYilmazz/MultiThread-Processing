/*
 * minion.c
 *
 *  Created on: May 2, 2017
 *      Author: sprayo7
 */

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "minion.h"

int main(int args, char *argv[]) {
	printf("*******\n%s - %s\n****************\n", argv[1], argv[2]);
	char name_of_written_file[30];
	FILE *file_write; /*File that belongs to minionN.txt */
	int named_pipe = open(argv[2], O_RDONLY); /*Opening communication named pipe that coming with argument */
	controller_thread_parameter *thread =
			(controller_thread_parameter *) malloc(
					sizeof(controller_thread_parameter));

	printf("Receiver...: ");
	sprintf(name_of_written_file, "%s.txt", argv[1]);

	file_write = fopen(name_of_written_file, "w");
	fprintf(file_write,
			"Search for \"%s\" string in %s\n----------------------------------\n",
			argv[3], argv[1]);

	while (1) {
		/**
		 * There are two controls of isProducerDone situations, since
		 * if pipe is empty read() is hanging,
		 * other situation is to determine if producer is no longer generates for the pipe.
		 */
		if (thread->isProducerDone == 1) {
			printf("%s is ended\n", argv[1]);
			fclose(file_write);
			return 0;
		}

		read(named_pipe, thread, sizeof(controller_thread_parameter));
		/*Other situation part of above explanation */
		if (thread->isProducerDone == 1) {
			printf("%s is ended\n", argv[1]);
			return 0;
		}
		printf("Minion - %s:: %s, From thread %ld \n", argv[2], thread->path,
				thread->thread_id);
		readFile(thread->path, argv[3], file_write);

	}


	return 0;
}

/*Traversing all of the file and looking for the wordLike.
 * FILE argument is for the file to write.
 */
void readFile(char *path, char *wordLike, FILE *file_write) {
	FILE *file;

	char controlCharacter;
	word_info *word_information = (word_info *) malloc(sizeof(word_info));
	int counterForWords = 0;
	int columnCounter = 1;
	char word[100];
	char *resultWord;
	int position;
	int rowCounter = 1;


	file = fopen(path, "r");
	while ((controlCharacter = fgetc(file)) != EOF) {/*Reading file character by character. */
		/*If the character is a lowercase or uppercase or ' ' or '\n' or '-' character, then proceed them.
		 If it is not then continue reading characters from file.  */
		if ((controlCharacter >= 65 && controlCharacter <= 90)
				|| (controlCharacter >= 97 && controlCharacter <= 127)
				|| (controlCharacter >= 33 && controlCharacter <= 64)
				|| controlCharacter == ' ' || controlCharacter == '\n'
				|| controlCharacter == '-' || controlCharacter == '\t'
				|| controlCharacter == '\r') {
			if (controlCharacter == '\n') {
				rowCounter++;
				columnCounter = 0;
			}

			if (controlCharacter == ' ' || controlCharacter == '\n'
					|| controlCharacter == '\t' || controlCharacter == '\r') {/*If there is a whitespace or new line(means end of word)
					 it ends the process of creating word and send it to tree to add  */

				/*If a word is completed. */
				word[counterForWords] = '\0';
				counterForWords = 0;
				/**
				 * To determine index of the word, first it found the substring's palce
				 * where the word starts containing it then subtract it. Adding the column their result.
				 */
				resultWord = strstr(convertToLowerCharacter(word),
						convertToLowerCharacter(wordLike));
				if (resultWord != NULL) {
					position = compareTo(convertToLowerCharacter(wordLike), convertToLowerCharacter(word)) + columnCounter;
					fprintf(file_write, "%s:%d:%d\n", path, rowCounter,
							position);
					strcpy(word_information->filePath, path);
					word_information->wordColumn = columnCounter;
					word_information->wordRow = rowCounter;
				}
				columnCounter += strlen(word) + 1;

			} else {
				word[counterForWords++] = controlCharacter;/*If characters are still coming, it adds them in a string. */
			}
		}
	}
	/*If the last word int the file is not read, it is controlling for it. */
	word[counterForWords] = '\0';
	resultWord = strstr(word, wordLike);
	if (resultWord != NULL) {

		position = resultWord - word + columnCounter;
		printf("Word %s - column %d - row %d\n", word, columnCounter,
				rowCounter);
		strcpy(word_information->filePath, path);
		word_information->wordColumn = columnCounter;
		word_information->wordRow = rowCounter;
	}
}

/*Converting a string to lowercase to comparison. */
char *convertToLowerCharacter(char *data) {
	int i = 0;
	char *lowerData = (char *) malloc((strlen(data) + 1) * sizeof(char));

	while (data[i]) {
		lowerData[i] = tolower(data[i]);
		i++;
	}
	lowerData[i] = '\0';
	return lowerData;
}

/*When I tried to find result - s2 situation in the origin function,
 * I get unstable results since there is subtraction between two adress char.
 *  This method creates, copies and uses the strings that will be compared as local and
 *  without unstable results.
 */
int compareTo(char *string1, char *string2) {
	char *s1 = (char *)malloc((strlen(string1) + 1) * sizeof(char));
	char *s2 = (char *)malloc((strlen(string2) + 1) * sizeof(char));
	strcpy(s1, string1);
	strcpy(s2, string2);
	char *result = strstr(s2, s1);
	free(s1);
	free(s2);
	return result - s2;
}


