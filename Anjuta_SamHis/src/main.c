/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Unknown 2010 <wei@localhost.localdomain>
 * 
 * SampHis is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * SampHis is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**********************************************************************
* file:   contents copied from previous sample work
*         try to add history information
* date:   Mon May 11 2010  
* Author: Wei Wang
*
* Description: 
* Integrate History Information to program
*
*
* Compile with:
* 
* Usage:
* 
* 
**********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>     /* SIGINT */
#include <sys/stat.h>   /* stat() */
#include <unistd.h>
#include <time.h>    
#include <assert.h>     /* assert()*/
#define MAX_INT 65535

struct timeval start;
struct timeval end;
double est_total;
double est_num;

/* New struct for saving history */
struct dir_node
{
	int sub_file_num;
	int sub_dir_num;
	
	int bool_dir_covered;
	char *dir_name;				/* rather than store the subdir name */
	struct dir_node *sdirStruct; /* child array dynamically allocated */
};

struct dir_node root;


void CleanExit(int sig);
void get_all_subdirs
(const char *path, struct dir_node *, int *, int *);
static char *dup_str(const char *s);
double GetResult();
int begin_sample_from(const char *root, struct dir_node *);
int random_next(int random_bound);

int main(int argc, char **argv) 
{
	unsigned int sample_times;
	size_t i;
	struct dir_node *curPtr;
	
	signal(SIGKILL, CleanExit);
	signal(SIGTERM, CleanExit);
	signal(SIGINT, CleanExit);
	signal(SIGQUIT, CleanExit);
	signal(SIGHUP, CleanExit);

  
	/* start timer */
	gettimeofday(&start, NULL ); 

	if (argc < 3)
	{
		printf("Usage: %s drill-down-times pathname\n",argv[0]);
		return EXIT_FAILURE; 
	}
	if (chdir(argv[2]) != 0)
	{
		printf("Error when chdir to %s", argv[2]);
		return EXIT_FAILURE; 
	}

	sample_times = atoi(argv[1]);
	assert(sample_times <= MAX_INT);

	/* initialize the value */
	{
        est_total = 0;
        est_num = 0;
	}

	/* Initialize the dir_node struct */
	curPtr = &root;
	curPtr->bool_dir_covered = 0;
	curPtr->sdirStruct = NULL;
	
	srand((int)time(0));//different seed number for random function
	//srand can also be used like this
	//srand(2); 
	
	/* start sampling */
	for (i=0; i < sample_times; i++)
	{
		begin_sample_from(argv[2], curPtr);
		printf("there are on average %.2f files\n", GetResult());
	}

	chdir("/tmp");	  /* this is for the output of gprof */
	return EXIT_SUCCESS;
}

int random_next(int random_bound)
{
	assert(random_bound < RAND_MAX);
	return rand() % random_bound;	
}


int begin_sample_from(const char *root, struct dir_node *curPtr) 
{
	int sub_dir_num = 0;
	int sub_file_num = 0;
		
    //string curDict = dirstr;
	/* designate the current directory where sampling is to happen */
	char *cur_parent = dup_str(root);
	int bool_sdone = 0;
    double prob = 1;

		
    while (bool_sdone != 1)
    {
		sub_dir_num = 0;
		sub_file_num = 0;

		/* get all sub_dirs struct allocated and organized in to an array
		 * and curPtr's sdirStruct pointer already points to it */		
		get_all_subdirs(cur_parent, curPtr, &sub_dir_num, &sub_file_num);
		
		/* sdirStruct should not be null! */
		if (!curPtr->sdirStruct)
		{
			fprintf(stderr, "something went wrong in getting dirs\n");
			return EXIT_FAILURE;
		}                  
        sub_dir_num = curPtr->sub_dir_num;
		sub_file_num = curPtr->sub_file_num;
		
        est_total = est_total + (sub_file_num / prob);

		if (sub_dir_num > 0)
		{
			prob = prob / sub_dir_num;
			int temp = random_next(sub_dir_num);
			cur_parent = dup_str(curPtr->sdirStruct[temp].dir_name);
			curPtr = &curPtr ->sdirStruct[temp];		
		}
		
		/* leaf directory, end the drill down */
        else
        {
			est_num++;
			chdir(root);
			bool_sdone = 1;
        }
    } 
    return EXIT_SUCCESS;
}


/* Before the calling of the function, please ensure you have 
 * already CDed to the right place, so the function can directly
 * open the directory for the whole scanning
 * Open dir requires you give either the absolute path, or the relative path
 * to the current directory (which changes all the time)
 */
void get_all_subdirs(   
    const char *path,               /* path name of the parent dir */
    struct dir_node *curPtr,  /* */
	int *sub_dir_num,		        /* number of sub dirs */
	int *sub_file_num)		        /* number of sub files */
{
    DIR *dir;
    struct dirent *pdirent;
    size_t alloc;
	size_t used;
 	struct stat f_ftime;  /* distinguish files from directories */

	/* already stored the subdirs struct before
	 * no need to scan the dir again */
	if (curPtr->bool_dir_covered == 1)
	{
		chdir(path);
		return;
	}
		
	/* so we have to scan */
	/* root is given like the absolute path regardless of the cur_dir */
    if (!(dir = opendir(path)))
	{
		printf("current dir %s\n", get_current_dir_name());
		printf("error opening dir %s\n", path);
        //goto error;
		exit(-1);
    }

	/* and change to this directory */
	chdir(path);


 
    used = 0;
    alloc = 50;
    if (!(curPtr->sdirStruct
			= malloc(alloc * sizeof (struct dir_node)) )) 
	{
        //goto error_close;
		printf("malloc error!\n");
		exit(-1);
    }

	/* scan the directory */
    while ((pdirent = readdir(dir))) 
	{
		if(strcmp(pdirent->d_name, ".") == 0 ||
				    strcmp(pdirent->d_name, "..") == 0) 
		  continue;
		
		if(stat(pdirent->d_name, &f_ftime) != 0) 
		{   
			printf("stat error\n");			
			return;
		}
		/* currently only deal with regular file and directory 
		 * are there any other kind ?
		 */
		if (S_ISREG(f_ftime.st_mode))
			(*sub_file_num)++;

		/* record the sub direcotry names 
		 * this would contain the hidden files begin with "."
		 */
		else if (S_ISDIR(f_ftime.st_mode)) 
		{
    		if (used + 1 >= alloc) 
			{
        		size_t new = alloc / 2 * 3;
        		struct dir_node *tmp = 
					realloc(curPtr->sdirStruct, new * sizeof (struct dir_node));
        		if (!tmp) 
				{
            		//goto error_free;
					printf("realloc error!\n");
					exit(-1);
        		}
        		curPtr->sdirStruct = tmp;
        		alloc = new;
    		}
    		if (!(curPtr->sdirStruct[used].dir_name = dup_str(pdirent->d_name))) 
			{
        		//goto error_free;
				printf("get name error!!!!\n");
    		}
    		++used;
		}

		/* other non-file, non-dir special files */
		else 
			continue;
	}
	*sub_dir_num = used;

	curPtr->sub_file_num = *sub_file_num;
	curPtr->sub_dir_num = *sub_dir_num;
	/* update bool_dir_covered info */
	curPtr->bool_dir_covered = 1;
    closedir(dir);

	 /* previous approach to prevent memory leak  
error_free:
    while (used--) 
	{
        free(s_dirs[used]);
    }
    free(s_dirs);
 
error_close:
    closedir(dir);
 
error:
    return NULL;   */
}

double GetResult()
{
	printf("est_total:%.2f, est_num:%.2f\n", est_total, est_num);
    return (est_total / est_num);
}


static char *dup_str(const char *s) 
{
    size_t n = strlen(s) + 1;
    char *t = malloc(n);
    if (t) 
	{
        memcpy(t, s, n);
    }
    return t;
}

void CleanExit(int sig)
{
    /* make sure everything that needs to go to the screen gets there */
    fflush(stdout);
    gettimeofday(&end, NULL ); 
    
    if (sig != SIGHUP)
        printf("\nExiting...\n");
    else
        printf("\nRestaring...\n");

    puts("\n\n=============================================================");
    printf("put results here\n");
	
    puts("\n\n=============================================================");
    printf("Total Time:%ld milliseconds\n", 
	(end.tv_sec-start.tv_sec)*1000+(end.tv_usec-start.tv_usec)/1000);
    printf("Total Time:%ld seconds\n", 
	(end.tv_sec-start.tv_sec)*1+(end.tv_usec-start.tv_usec)/1000000);
    exit(0);
}

