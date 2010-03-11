/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) GWU 2010 <wwang@gwu.edu>
 * 
 * FSample is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * FSample is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/**********************************************************************
* file:   contents copied from fsample.c
* date:   Mon May 08 23:07:49 2010  
* Author: Wei Wang
*
* Description: 
* ImplementRandom Drill-down Sampling To Statistic The
* Number of files 
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
int  qcost;

void CleanExit(int sig);
static char **get_all_subdirs(const char *path, int *, int *);
static char *dup_str(const char *s);
int GetCost();
double GetResult();
int begin_sample_from(const char *root);
char * random_next(char **dirs, int random_bound);

int main(int argc, char **argv) 
{
	unsigned int sample_times;
	size_t i;
	
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
		printf("Error when chdir to %s", argv[1]);
		return EXIT_FAILURE; 
	}

	sample_times = atoi(argv[1]);
	assert(sample_times <= MAX_INT);

	/* initialize the value */
	{
        est_total = 0;
        est_num = 0;
        qcost = 0;
	}
	/* start sampling */
	for (i=0; i < sample_times; i++)
	{
		begin_sample_from(argv[2]);
		printf("there are on average %.2f files\n", GetResult());
	}

	//printf("there are on average %.2f files\n", GetResult());
	return EXIT_SUCCESS;
}

char * random_next(char **dirs, int random_bound)
{
	assert(random_bound < RAND_MAX);
	return dirs[random() % random_bound];
	
}

/* Didn't deal with the following access exception in the Original CSharp 
				 catch (UnauthorizedAccessException)
                    {
                        sdone = true;
                        est_num++;
                        Console.Write("no right to access.");
                    }
*/
int begin_sample_from(const char *root) 
{
	int sub_dir_num = 0;
	int sub_file_num = 0;
	char **sub_dirs;
	size_t i;
	
	
    //string curDict = dirstr;
	/* designate the current directory where sampling is to happen */
	char *cur_parent = dup_str(root);
	int bool_sdone = 0;
    double prob = 1;

	srand((int)time(0));//different seed number for random function
		
    while (bool_sdone != 1)
    {
		sub_dir_num = 0;
		sub_file_num = 0;
		/*
		 string[] flist = Directory.GetFiles(curDict);
         string[] dlist = Directory.GetDirectories(curDict);

		 */
		sub_dirs = get_all_subdirs(cur_parent, &sub_dir_num, &sub_file_num);
		if (!sub_dirs)
		{
			fprintf(stderr, "something went wrong in getting dirs\n");
			return EXIT_FAILURE;
		}                  
                        
        qcost++;
        est_total = est_total + (sub_file_num / prob);

		if (sub_dir_num > 0)
		{
			//prob = prob / dlist.Length;
			//curDict = dlist[ThreadSafeRandom.Next(dlist.Length)];
			prob = prob / sub_dir_num;
			cur_parent = dup_str(random_next(sub_dirs, sub_dir_num));

			for (i = 0; sub_dirs[i]; ++i)
			{
				printf("%s\n", sub_dirs[i]);
			}
 
			for (i = 0; sub_dirs[i]; ++i) 
			{
				free(sub_dirs[i]);
			}
			free(sub_dirs);
		}
        else
        {
			est_num++;
			bool_sdone = 1;
        }
    } 
    return EXIT_SUCCESS;
}


int GetCost()
{
    return qcost;
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

/* Before the calling of the function, please ensure you have 
 * already CDed to the right place, so the function can directly
 * open the directory for the whole scanning
 * Open dir requires you give either the absolute path, or the relative path
 * to the current directory (which changes all the time)
 */
static char **get_all_subdirs(   
    const char *path,            /* path name of the parent dir */
	int *sub_dir_num,		    /* number of sub dirs */
	int *sub_file_num)		    /* number of sub files */
{
    DIR *dir;
    struct dirent *pdirent;
    char **s_dirs;
    size_t alloc;
	size_t used;
 	struct stat f_ftime;  /* distinguish files from directories */

	/* root is given like the absolute path regardless of the cur_dir */
    if (!(dir = opendir(path)))
	{
		printf("error opening dir\n");
        goto error;
    }

	/* and change to this directory */
	chdir(path);
 
    used = 0;
    alloc = 10;
    if (!(s_dirs = malloc(alloc * sizeof *s_dirs))) 
	{
        goto error_close;
    }

	/* scan the directory */
    while ((pdirent = readdir(dir))) 
	{
		if(strcmp(pdirent->d_name, ".") == 0 ||
				    strcmp(pdirent->d_name, "..") == 0) 
		  continue;
		
		if(stat(pdirent->d_name, &f_ftime) != 0) 
		  return -1;

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
        		char **tmp = realloc(s_dirs, new * sizeof *s_dirs);
        		if (!tmp) 
				{
            		goto error_free;
        		}
        		s_dirs = tmp;
        		alloc = new;
    		}
    		if (!(s_dirs[used] = dup_str(pdirent->d_name))) 
			{
        		goto error_free;
    		}
    		++used;
		}
		else 
			continue;
	}
	s_dirs[used] = NULL;
	*sub_dir_num = used;
    closedir(dir);
	return s_dirs;
 
error_free:
    while (used--) 
	{
        free(s_dirs[used]);
    }
    free(s_dirs);
 
error_close:
    closedir(dir);
 
error:
    return NULL;
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



