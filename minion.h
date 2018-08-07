/*
 * minion.h
 *
 *  Created on: Apr 30, 2017
 *      Author: Serhat YILMAZ
 */

#ifndef MINION_H_
#define MINION_H_

/*To get content of the data that comes from named pipe. */
typedef struct controller_paramater{
	long thread_id;
	int isProducerDone;
	char path[PATH_MAX];
}controller_thread_parameter;


typedef struct word_information{
	char filePath[PATH_MAX];
	int wordRow;
	int wordColumn;
}word_info;

/*Comparing two strings to determine string1's index. */
extern int compareTo(char *string1, char *string2);

/*Converting all the string to comparision. */
extern char *convertToLowerCharacter(char *data);

/*Reading file and finding searched word in the files. */
extern void readFile(char *path, char *, FILE *);
#endif /* MINION_H_ */
