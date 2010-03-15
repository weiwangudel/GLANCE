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
#include <sys/time.h>   /* gettimeofday() */
#include <time.h>		/* time() */
#include <assert.h>     /* assert()*/

#define MAX_DRILL_DOWN 1000000

struct timeval start;
struct timeval end;

double est_total;
double est_num;
long int already_covered = 0;
long int newly_covered = 0;
long int g_boundary_num = 10000;
int g_dq_times;		/* dq iteration loop */
long int g_dq_threshold;  /* dq threshold */
int g_large_used =0; /* large directory encountered  */
int g_large_alloc = 10; /* large directory initially allocated */
long int *g_large_array;  /* Stores the large array */ 

/* New struct for saving history */
struct dir_node
{
	long int sub_file_num;
	long int sub_dir_num;
	
	int bool_dir_covered;
	char *dir_name;				/* rather than store the subdir name */
	struct dir_node *sdirStruct; /* child array dynamically allocated */
};


void CleanExit(int sig);
void get_all_subdirs
(const char *path, struct dir_node *, int *, int *);
static char *dup_str(const char *s);
double GetResult();
int begin_sample_from(const char *root, struct dir_node *, double);
int random_next(int random_bound);
int check_type(const struct dirent *entry);
void fast_subdirs(const char *, struct dir_node *, long int *sub_dir_num, 
   long int *sub_file_num);
void anomaly_processing(struct dir_node *curPtr);
long int get_min_max(struct timeval begin, struct timeval end,
    	long int *min, long int *max);

/* why do I have to redefine to avoid the warning of get_current_dir_name? */
char *get_current_dir_name(void);

int main(int argc, char **argv) 
{
	long int sample_times;
	long int i;
	struct dir_node *curPtr;
	struct dir_node root;
	long int sample_min = 10000000; /* 10 seconds */
	long int sample_max = 0;
	struct timeval sample_start;
	struct timeval sample_end;
	
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

	sample_times = atol(argv[1]);
	assert(sample_times <= MAX_DRILL_DOWN);
	g_dq_threshold = atol(argv[3]);
	g_dq_times = atoi(argv[4]);
	/* initialize the value */
	{
        est_total = 0;
        est_num = 0;
	}

	/* Initialize the dir_node struct */
	curPtr = &root;
	curPtr->bool_dir_covered = 0;
	curPtr->sdirStruct = NULL;

	/* Initialize the global large array */
	if (!(g_large_array = malloc(g_large_alloc * sizeof (long int)) ))
	{
		printf("malloc g_large_array error!\n");
		exit(-1);
	}
	
	srand((int)time(0));//different seed number for random function
	//srand can also be used like this
	//srand(2); 

	int * depth_array = malloc(sample_times * sizeof (int));
	long int * time_array = malloc (sample_times * sizeof (long int ));
	
	/* start sampling */
	for (i=0; i < sample_times; i++)
	{		
		gettimeofday(&sample_start, NULL);
		depth_array[i] = begin_sample_from(argv[2], curPtr, 1.0);
		gettimeofday(&sample_end, NULL);		

		time_array[i] =
			get_min_max(sample_start, sample_end, &sample_min, &sample_max);
	}

	chdir("/tmp");	  /* this is for the output of gprof */

	/* Exit and Display Statistic */
//	printf("Min drill down time:%ld microseconds\n", sample_min);
	//printf("Max drill down time:%ld microseconds\n", sample_max);

	for (i=0; i < sample_times; i++)
		printf("%ld\t%d\t%ld\n", i, depth_array[i], time_array[i]);
	CleanExit (2);

	return EXIT_SUCCESS;
}

long int get_min_max(struct timeval sample_begin, struct timeval sample_end,
    	long int *min, long int *max)
{
	long int temp;
	temp = (sample_end.tv_sec-sample_begin.tv_sec)*1000000
		+ (sample_end.tv_usec-sample_begin.tv_usec);
	
	if (temp > *max)
		*max = temp;
	if (temp < *min)
		*min = temp;
	return temp;
}

int random_next(int random_bound)
{
	assert(random_bound < RAND_MAX);
	return rand() % random_bound;	
}

/* to ensure accuray, the root parameter should be passed as 
 * absolute!!!! path, so every return would 
 * start from the correct place
 */
int begin_sample_from(
		const char *sample_root, 
		struct dir_node *curPtr,
		double old_prob) 
{
	long int sub_dir_num = 0;
	long int sub_file_num = 0;
	int depth = 0;	

	/* designate the current directory where sampling is to happen */
	char *cur_parent = dup_str(sample_root);
	int bool_sdone = 0;

	double prob = old_prob;
	double temp_prob; 
		
    while (bool_sdone != 1)
    {
		sub_dir_num = 0;
		sub_file_num = 0;

		fast_subdirs(cur_parent, curPtr, &sub_dir_num, &sub_file_num);

		/* sdirStruct not null sounds not necessary to check! */
		/*if (!curPtr->sdirStruct) */              
        
		/* anomaly detection */
		anomaly_processing(curPtr);

		sub_dir_num = curPtr->sub_dir_num;
		sub_file_num = curPtr->sub_file_num;


	    est_total = est_total + (sub_file_num / prob);
		/*printf("Under %s, the prob is %f,/number of files is %ld,I added %lf \
		    files to est_total\n",
		    get_current_dir_name(), prob, sub_file_num, sub_file_num / prob);*/

		if (sub_dir_num > 0)
		{
			temp_prob = prob / sub_dir_num;
			
			if (temp_prob < old_prob / g_dq_threshold)
			{
				int i;
				//printf("test!!!!!!\n");

				for (i = 0; i < g_dq_times; i++)			
				{
					//printf("in D&Q %s\n", get_current_dir_name());
					begin_sample_from(get_current_dir_name(), curPtr,
				    					    prob*g_dq_times);
				}
				if (((int) old_prob) == 1)
					est_num++;
				chdir(sample_root);
				bool_sdone = 1;
				continue;
			}
			prob = temp_prob;	
			
			int temp = random_next(sub_dir_num);
			cur_parent = dup_str(curPtr->sdirStruct[temp].dir_name);
			curPtr = &curPtr ->sdirStruct[temp];	

			depth++;
		}
		
		/* leaf directory, end the drill down */
        else
        {
			if (((int) old_prob) == 1)
				est_num++;

			/* finishing this drill down, set the direcotry back */
			chdir(sample_root);
			bool_sdone = 1;
			
        }
    } 
    return depth;
}

void anomaly_processing(struct dir_node *curPtr)
{
	if (curPtr->sub_file_num > g_boundary_num)
	{
		/* record the large sub_file_num then set it to 0 */
		if (g_large_used + 1 >= g_large_alloc)
		{
			size_t new = g_large_alloc /2 *3;
			long int *tmp = 
				realloc(g_large_array, new * sizeof (long int));
			if (!tmp)
			{
				printf("Realloc g_large_array error!\n");
				exit(-1);
			}

			g_large_array = tmp;
			g_large_alloc = new;
		}

		/* store sub_file_num to array */
		g_large_array[g_large_used] = curPtr->sub_file_num;
		
		++g_large_used;	
		curPtr->sub_file_num = 0;
	}	
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
		/* This change dir is really important */
		already_covered++;
		chdir(path);
		//printf("cur dir:%s\n", get_current_dir_name());
		return;
	}
		
	/* so we have to scan */
	newly_covered++;
	
	/* root is given like the absolute path regardless of the cur_dir */
    if (!(dir = opendir(path)))
	{
		//printf("current dir %s\n", get_current_dir_name());
		printf("error opening dir %s\n", path);
        //goto error;
		exit(-1);
    }

	/* and change to this directory */
	chdir(path);
 
	//printf("cur dir:%s\n", get_current_dir_name());
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

}

double GetResult()
{
	//printf("est_total:%.2f, est_num:%.2f\n", est_total, est_num);
    return (est_total / est_num);
}

double GetLargeValue()
{
	size_t i;
	double sum_file_num = 0;
	for (i = 0; i < g_large_used; i++)
		sum_file_num += g_large_array[i];
	return sum_file_num;	
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
	printf("Not that if last unfinished, est_num is counted as finished\n");
	printf("Thus the correct result should be higher than the above value\n");
	printf("\ndirs newly opened %ld\ndirs already_covered %ld\n",
			newly_covered, already_covered);
	printf("\"Note that the newly opened dirs should not exceed the total \
number of directories existing there\"\n");
	printf("there are on average %.2f files\n", GetResult()+GetLargeValue ());
	printf("large dir has in all %.2f files\n", GetLargeValue ());
    puts("=============================================================");
    printf("Total Time:%ld milliseconds\n", 
	(end.tv_sec-start.tv_sec)*1000+(end.tv_usec-start.tv_usec)/1000);
    printf("Total Time:%ld seconds\n", 
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
		already_covered++;
		chdir(path);

		return;
	}
		
	/* so we have to scan */
	newly_covered++;	

    total_num = scandir(path, &namelist, 0, 0);
	
	/* root is given like the absolute path regardless of the cur_dir */
    (*sub_dir_num) = scandir(path, &namelist, check_type, 0);
	chdir(path);
	
	*sub_file_num = total_num - *sub_dir_num;
 	alloc = *sub_dir_num - 2;
	used = 0;

 	assert(alloc >= 0);

    if (alloc > 0 && !(curPtr->sdirStruct
			= malloc(alloc * sizeof (struct dir_node)) )) 
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
   		if (!(curPtr->sdirStruct[used++].dir_name 
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

