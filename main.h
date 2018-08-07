/*
 * main.h
 *
 *  Created on: May 2, 2017
 *      Author: Serhat YILMAZ
 */

#ifndef MAIN_H_
#define MAIN_H_
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
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

/* Struct for buffer, buffer is constructed over a FIFO sturcture. */
typedef struct queue {
	char filePath[PATH_MAX];
	struct queue *next;
} Buffer;

/*It is sent to minion process to extract information inside. */
typedef struct controller_paramater {
	long thread_id;
	int isProducerDone;
	char path[PATH_MAX];
} controller_thread_parameter;

/*Every minion is built a named pipe to comminicate with its contoller thread. */
extern int *connection_pipe;

/*Consumer function is used by controller thread */
extern void *consumer(void *);

/*When no more path to write to buffer by file searcher thread,
 * it is end to pipe and end to minion process */
extern void endThePipe(long id);

/* To synchronize the buffer */
extern pthread_mutex_t buffer_mutex;

/*When buffer is full it waits to consumer signal */
extern pthread_cond_t full_cond;

/*When no item in the buffer, it waits for the buffer to signal it */
extern pthread_cond_t empty_cond;

/* Argument entry which determine the buffer size */
extern int BUFFER_SIZE;

/*Counting buffer to determine when the buffer full or empty. */
extern int bufferCount;

/*When file searcher thread is done, it is ending process flag. */
extern int isProducerDone;

/*Called by consumer, dequeue from queue and write it to connection pipe with minion process. */
extern void dequeue(long id);

/*Work with file searcher thread. When a new path is found it is added to last place on the queue */
extern void enqueue(char *);
extern int isEmpty();

/*Producer loop calls it. */
extern void *producer(void *);

/*Determining whether the file txt file or not */
extern int isTxt(char *path);


extern void directoryList(void *cPath);

/*To determine the path is a file or directory. */
extern int isDirectory(char *path);

/* Read all directories with recursion */
extern void readText(char *path, char *fileName, int mode);

/* Since searching works recursively, this process controlled by two functions
 * to synchronize them. */
extern void *producer_loop(void *path);

#endif /* MAIN_H_ */
