#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "main.h"

/*Global variables' explanations are in 'main.h' */

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty_cond = PTHREAD_COND_INITIALIZER;

Buffer *first;
Buffer *last;

int *connection_pipe;
int BUFFER_SIZE ;
int bufferCount = 0;
int isProducerDone = 0;
int numberOfMinion;


int main(int argc, char *argv[]) {
	char pipe_name[10]; /*Pipe names associated with the its minion's name */
	char minion_name[20]; /*When a minion is created, it is named with an id */
	long i; /* loop counter */
	BUFFER_SIZE = atoi(argv[2]); /*Determining buffer size */
	char *theWordWillBeContained = argv[3]; /*Determining to be searched word. */
	numberOfMinion = atoi(argv[1]); /*Determining the minion number */
	int minion_id[numberOfMinion]; /*For fork() process. */

	/*There should exist connection pipe as the number of minion to
	 * communicate each other with private pipe. */
	connection_pipe = (int *) malloc(numberOfMinion * sizeof(int));

	/*Creating named pipes for all minions */
	for (i = 0; i < numberOfMinion; i++) {
		sprintf(pipe_name, "p%ld", i);
		printf("%ld.pipe Name: %s\n", i + 1, pipe_name);
		mkfifo(pipe_name, 0666);
	}

	/*Forking and exec() processes. */
	for (i = 0; i < numberOfMinion; i++) {
		minion_id[i] = fork();
		if (minion_id[i] == 0) {
			sprintf(minion_name, "minion%ld", i + 1);
			sprintf(pipe_name, "p%ld", i);
			/*While executing new program by child process
			 * it sends some arguments */
			char *ar[] = { "./minion", minion_name, pipe_name, theWordWillBeContained, NULL };
			printf("%s created\n", minion_name);
			execv("./minion", ar);
		}
	}

	/*Opening pipes to comminicate controller thread and minion processes program */
	for (i = 0; i < numberOfMinion; i++) {
		sprintf(pipe_name, "p%ld", i);
		printf("%ld.written pipe:: %s\n", i, pipe_name);
		connection_pipe[i] = open(pipe_name, O_WRONLY);
	}

	/************************************************************
	 * 				      THREADS CREATION					 ****
	 ************************************************************/
	pthread_t producerT;
	pthread_t controllerThreads[numberOfMinion];

	pthread_create(&producerT, NULL, &producer_loop, (void *) argv[4]);

	for (i = 0; i < numberOfMinion; i++) {
		pthread_create(&controllerThreads[i], NULL, &consumer, (void *) i);
	}

	pthread_join(producerT, NULL);
	for (i = 0; i < numberOfMinion; i++) {
		pthread_join(controllerThreads[i], NULL);
	}

	return 0;
}

/*When file searcher thread is done it writes the called pipe ending flag. */
void endThePipe(long id) {
	controller_thread_parameter *enderThread =
			(controller_thread_parameter *) malloc(
					sizeof(controller_thread_parameter));
	enderThread->isProducerDone = 1;
	write(connection_pipe[id], enderThread, sizeof(controller_thread_parameter));

}

/*Since directory searching method runs recursively, it should
 * be executed with tow functions to avoid mutex deadlocks */
void *producer_loop(void *path) {

	pthread_mutex_lock(&buffer_mutex);
	producer(path);

	printf("isProducer now 1\n");
	isProducerDone = 1;
	pthread_cond_signal(&full_cond);
	pthread_mutex_unlock(&buffer_mutex);
}

/**
 * Searching for txt files.
 */
void *producer(void *path) {
	/* DECLARATIONS */
	char *cPath;
	DIR *directory;
	struct dirent *directoryFeatures; /*To hold directories' features. */
	char *directoryName;
	char newPath[PATH_MAX];
	int pathLength;
	/* PROCESSES */
	cPath = (char *) path;
	directory = opendir(cPath);
	if (directory == NULL) {
		fprintf(stderr, "ERROR : Directory can't be opened: %s.\n", cPath);
		exit(EXIT_FAILURE);
	}

	/*It is a loop for iterating directories */
	while (1) {

		directoryFeatures = readdir(directory);
		if (directoryFeatures == NULL) { /*If there is no more directory to read, then exit from loop */
			break;
		}
		directoryName = directoryFeatures->d_name;
		sprintf(newPath, "%s/%s", cPath, directoryFeatures->d_name); /*The function runs recursively. Therefore
		 it needs next directory or file if it exists.*/
		if (isDirectory(newPath) == 1) {
			if (strcmp(directoryFeatures->d_name, ".") == 0
					|| strcmp(directoryFeatures->d_name, "..") == 0) {
				continue;
			}
			sprintf(newPath, "%s/%s", cPath, directoryFeatures->d_name);
			pathLength = strlen(newPath) + 1;
			if (pathLength >= PATH_MAX) {
				fprintf(stderr, "ERROR : Path length is too long.\n");
				exit(EXIT_FAILURE);
			}
			producer((void *) newPath); /*Recursion part of function. */

		} else {/*If the read part of directory is not a directory then it is sent to function that proceed it  */
			if (bufferCount == BUFFER_SIZE) {
				/*When buffer is full, it waits for empty signal from consumer method */
				pthread_cond_wait(&empty_cond, &buffer_mutex);

			}
			sprintf(newPath, "%s/%s", cPath, directoryName);
			readText(cPath, directoryName, 1);
		}
		/*After adding a new path to queue, it releases the mutex for contoller threads.*/
		pthread_cond_signal(&full_cond);

		pthread_mutex_unlock(&buffer_mutex);
		sleep(1);
	}

}

/**
 * Consumer method for controller threads. When it is called, it lock mutex, control
 * buffer, and write to first item on the queue to the related pipe with dequeue() method.
 */
void *consumer(void *id) {
	long tid = (long) id;
	while (1) {

		pthread_mutex_lock(&buffer_mutex);
		if (bufferCount == 0) {/* When buffer has no item, it waits for file searcher thread */

			pthread_cond_wait(&full_cond, &buffer_mutex);

			/* When producer thread is done, controller threads must not wait for
			 * a new item anymore. Therefore isProduceDone is a trigger variable
			 * to control ending a controller thread.f */
			if (isProducerDone == 1) {

				endThePipe(tid);
				pthread_cond_signal(&full_cond);
				pthread_mutex_unlock(&buffer_mutex);
				pthread_exit(NULL);
			}
		}

		dequeue(tid);

		pthread_mutex_unlock(&buffer_mutex);
		sleep(1);
	}
}

/*To read entered directory and files and directories inside it. */
void directoryList(void *cPath) {

	/* DECLARATIONS */
	char *path;
	DIR *directory;
	struct dirent *directoryFeatures; /*To hold directories' features. */
	char *directoryName;
	char newPath[PATH_MAX];
	int pathLength;
	/* PROCESSES */
	path = (char *) cPath;
	directory = opendir(path);
	if (directory == NULL) {
		fprintf(stderr, "ERROR : Directory can't be opened: %s.\n", path);
		exit(EXIT_FAILURE);
	}
	/*It is a loop for iterating directories */
	while (1) {
		directoryFeatures = readdir(directory);
		if (directoryFeatures == NULL) { /*If there is no more directory to read, then exit from loop */
			break;
		}
		directoryName = directoryFeatures->d_name;
		sprintf(newPath, "%s/%s", path, directoryFeatures->d_name); /*The function runs recursively. Therefore
		 it needs next directory or file if it exists.*/
		if (isDirectory(newPath) == 1) {
			if (strcmp(directoryFeatures->d_name, ".") == 0
					|| strcmp(directoryFeatures->d_name, "..") == 0) {
				continue;
			}
			sprintf(newPath, "%s/%s", path, directoryFeatures->d_name);
			pathLength = strlen(newPath) + 1;
			if (pathLength >= PATH_MAX) {
				fprintf(stderr, "ERROR : Path length is too long.\n");
				exit(EXIT_FAILURE);
			}
			directoryList(newPath); /*Recursion part of function. */

		} else {/*If the read part of directory is not a directory then it is sent to function that proceed it  */
			sprintf(newPath, "%s/%s", path, directoryName);
			readText(path, directoryName, 1);
		}
	}
}

/*It controls the path if it is directory or a file */
int isDirectory(char *path) {
	struct stat status;
	stat(path, &status);
	return S_ISDIR(status.st_mode);
}

/*
 *	 To add new item to queue if buffer is not full.
*/
void readText(char *path, char *fileName, int mode) {

	char pathOfFile[300];
	sprintf(pathOfFile, "%s/%s", path, fileName);
	if (isTxt(pathOfFile) == 1) {
		if (BUFFER_SIZE > bufferCount) {


			printf("Yazıyor\n");
			enqueue(pathOfFile);

			printf("%d. Txt file: %s\n", ++bufferCount, pathOfFile);
		}
	}

}

int isTxt(char *path) {
	int length = strlen(path);
	if (strcmp(".txt", &path[length - 4]) == 0) {
		return 1;
	}
	return 0;
}

/* Queue Functions  */

/*It is adding a new item to queue */
void enqueue(char *path) {
	Buffer *temp = (struct queue *) malloc(sizeof(struct queue));
	strcpy(temp->filePath, path);
	temp->next = NULL;
	if (first == NULL && last == NULL) {
		first = last = temp;
	} else {
		last->next = temp;
		last = temp;
	}
}

/* It is one of main functions to write and delete from queue.  */
void dequeue(long id) {
	Buffer *temp = first;

	/*Thread id and isProducerDone is too important for process management in the program. */
	/*Since thread id will specify the pipe and isProducerDone determines the minion process should end or not. */
	controller_thread_parameter *param = (controller_thread_parameter *) malloc(
			sizeof(controller_thread_parameter));
	param->thread_id = id;
	param->isProducerDone = 0;
	char pipe_name[20];
	if (first == NULL) {
		return;

		/*The following else if and else conditions tells that
		 * if all conditions are provided then it is writes to pipe and delete it from the queue.
		 * Most important part is to determine name of communication pipe
		 * with created by sprintf() function.
		 */
	} else if (first == last) {
		first = last = NULL;
		printf("HEY Consumer make this %d:: ", --bufferCount);
		strcpy(param->path, temp->filePath);
		sprintf(pipe_name, "p%ld", id);
		write(connection_pipe[id], param, sizeof(controller_thread_parameter));
		printf("HEY Thread %ld is write %s\n", id, param->path);
	} else {
		first = first->next;
		printf("HEY Consumer make this %d:: ", --bufferCount);
		strcpy(param->path, temp->filePath);
		sprintf(pipe_name, "p%ld", id);

		write(connection_pipe[id], param, sizeof(controller_thread_parameter));
		printf("HEY Thread %ld is write %s\n", id, param->path);
	}
	free(temp);
}

int isEmpty() {
	return first == NULL;
}
