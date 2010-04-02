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

const long int MAX_DRILL_DOWN =  1000000000;

struct timeval start;
struct timeval end;
double est_total;
double est_num;
long int g_dir_visited = 0;
double g_qcost_thresh;  /* exit upon percentage */
int g_reach_thresh = 0; /* flag whether reached the thresh */
/* New struct for saving history */
struct dir_node
{
	long int sub_file_num;
	long int sub_dir_num;
	
	int bool_dir_covered;
	char *dir_name;				/* rather than store the subdir name */
	struct dir_node *sdirStruct; /* child array dynamically allocated */
};

struct dir_node root;


void CleanExit(int sig);
void fast_subdirs(const char *path, struct dir_node *curPtr,  
		    long int *sub_dir_num, long int *sub_file_num);
static char *dup_str(const char *s);
int begin_sample_from(const char *root, struct dir_node *);
int random_next(int random_bound);
int check_type(const struct dirent *entry);

int main(int argc, char **argv) 
{
	long int sample_times;
	size_t i;
	struct dir_node *curPtr;
	long int real_dir = 1;
	long int real_file = 1;
	
	signal(SIGKILL, CleanExit);
	signal(SIGTERM, CleanExit);
	signal(SIGINT, CleanExit);
	signal(SIGQUIT, CleanExit);
	signal(SIGHUP, CleanExit);

  
	/* start timer */
	gettimeofday(&start, NULL ); 

	if (argc < 6)
	{
		printf("Usage: %s drill-down-times pathname\n",argv[0]);
		printf("realdir realfile threshold(percentage)\n");
		return EXIT_FAILURE; 
	}
	if (chdir(argv[2]) != 0)
	{
		printf("Error when chdir to %s", argv[2]);
		return EXIT_FAILURE; 
	}

	real_dir = atol(argv[3]);
	real_file = atol(argv[4]);
	g_qcost_thresh =  atof(argv[5]) * real_dir;

	printf("%s\t", get_current_dir_name());

	sample_times = atoi(argv[1]);
	assert(sample_times <= MAX_DRILL_DOWN);
	printf("%ld\t", sample_times);

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
	
	/* start sampling */
	for (i=0; i<sample_times && g_reach_thresh!=1; i++)
	{
		begin_sample_from(argv[2], curPtr);
	}

	/* so we drilled down i times */
	assert(i != 0);

	printf("%.6f\t", abs (est_total/i - real_file) * 1.0 / real_file);
	printf("%.6f\t", g_dir_visited * 1.0 / real_dir);

	CleanExit(2);
}

int random_next(int random_bound)
{
	assert(random_bound < RAND_MAX);
	return rand() % random_bound;	
}


int begin_sample_from(const char *root, struct dir_node *curPtr) 
{
	long int sub_dir_num = 0;
	long int sub_file_num = 0;
		
    //string curDict = dirstr;
	/* designate the current directory where sampling is to happen */
	char *cur_parent = dup_str(root);
	int bool_sdone = 0;
    double prob = 1;

		
    while (bool_sdone != 1)
    {
		sub_dir_num = 0;
		sub_file_num = 0;

		fast_subdirs(cur_parent, curPtr, &sub_dir_num, &sub_file_num); 	
        
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
    gettimeofday(&end, NULL ); 

    printf("%ld\n", 
	(end.tv_sec-start.tv_sec)*1+(end.tv_usec-start.tv_usec)/1000000);
    exit(0);
}


void fast_subdirs(   
    const char *path,               /* path name of the parent dir */
    struct dir_node *curPtr,  /* */
	long int *sub_dir_num,		        /* number of sub dirs */
	long int *sub_file_num)		        /* number of sub files */
{
    struct dirent **namelist;
    
    size_t alloc;
    int total_num;
	int used = 0;

	/* already stored the subdirs struct before
	 * no need to scan the dir again */
	if (curPtr->bool_dir_covered == 1)
	{
		/* This change dir is really important */
		chdir(path);
		return;
	}
		
	/* so we have to scan */
	g_dir_visited++;	
	if (g_dir_visited > g_qcost_thresh)
	{
		g_reach_thresh = 1;
	}

    total_num = scandir(path, &namelist, 0, 0);
	
	/* root is given like the absolute path regardless of the cur_dir */
    (*sub_dir_num) = scandir(path, &namelist, check_type, 0);
	chdir(path);
	
	*sub_file_num = total_num - *sub_dir_num;
 	alloc = *sub_dir_num - 2;
	used = 0;

 	assert(alloc >= 0);

    if (alloc > 0 && !(curPtr->sdirStruct
			= malloc(alloc * sizeof (struct dir_node )) )) 
	{
        //goto error_close;
		printf("malloc error!\n");
		exit(-1);
    }
    
    int temp = 0;
	/* scan the namelist */
    for (temp = 0; temp < *sub_dir_num; temp++)
    {
		if ((strcmp(namelist[temp]->d_name, ".") == 0) ||
                        (strcmp(namelist[temp]->d_name, "..") == 0))
               continue;
   		if (!((curPtr->sdirStruct[used++]).dir_name 
		       = dup_str(namelist[temp]->d_name))) 
		{
			printf("get name error!!!!\n");
    	}
		
	}

	*sub_dir_num -= 2;

	curPtr->sub_file_num = *sub_file_num;
	curPtr->sub_dir_num = *sub_dir_num;
	
	/* update bool_dir_covered info */
	curPtr->bool_dir_covered = 1;

}


int check_type(const struct dirent *entry)
{
    if (entry->d_type == DT_DIR)
        return 1;
    else
        return 0;
}

