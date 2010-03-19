/* Programmed by wei wang
 * Directed by professor Howie
 *
 * March 1, 2010
 *
 * Generate a filesystem
 ***********************************************************************/
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

#include "level_queue.h"

#include <fcntl.h>
#include <malloc.h>

#define MAX_PERMU 1000000
#define MAX_DRILL_DOWN 1000000

/* New struct for saving history */
struct dir_node
{
	long int sub_file_num;
	long int sub_dir_num;
    double factor;	
	int bool_dir_covered;
	char *dir_abs_path;	 /* absolute path, needed for BFS */			
	struct dir_node *sdirStruct; /* child array dynamically allocated */
};
struct timeval start;
struct timeval end;

double est_total;
long int sample_times;
int qcost = 0;
long int already_covered = 0;
long int newly_covered = 0;
long int g_boundary_num = 1000000000;
int g_dq_times;		/* dq iteration loop */
long int g_dq_threshold;  /* dq threshold */
int g_large_used =0; /* large directory encountered  */
int g_large_alloc = 10; /* large directory initially allocated */
long int *g_large_array;  /* Stores the large array */ 
long int g_folder;
long int g_file;
void CleanExit(int sig);
static char *dup_str(const char *s);
double GetResult();
int begin_estimate_from(struct dir_node *rootPtr);
int random_next(int random_bound);
int check_type(const struct dirent *entry);
void fast_subdirs(struct dir_node *curDirPtr);
void anomaly_processing(struct dir_node *, double prob);

/* why do I have to redefine to avoid the warning of get_current_dir_name? */
char *get_current_dir_name(void);
double floor(double);
int ar[MAX_PERMU];
void swap(int *a, int *b);
void permutation(int size);
struct queueLK level_q;

struct dir_node root_dir; /* root directory for estimation */


char proc_working_dir[100];


int main(int argc, char* argv[]) 
{
	long int i;
	struct dir_node *rootPtr;
	struct dir_node root_dir;
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

	g_folder = atol(argv[3]);
	g_file = atol(argv[4]);

	/* initialize the value */
	{
        est_total = 0;
        initQueue(&level_q);
	}

	/* Initialize the dir_node struct */
	rootPtr = &root_dir;
	rootPtr->bool_dir_covered = 0;
	rootPtr->sdirStruct = NULL;
    rootPtr->dir_abs_path = dup_str(argv[2]);

	int seed;
	srand(seed = (int)time(0));//different seed number for random function

	//srand can also be used like this
	//srand(2); 

	double * est_array = malloc(sample_times * sizeof (double));
	long int * qcost_array = malloc (sample_times * sizeof (long int ));
	
	/* start estimation */
	for (i=0; i < sample_times; i++)
	{		
		gettimeofday(&sample_start, NULL);
	 	begin_estimate_from(rootPtr);
		est_array[i] = est_total;
		qcost_array[i] = qcost;
		est_total = 0;
	        qcost = 0;		
		rootPtr->bool_dir_covered = 0;
		rootPtr->sdirStruct = NULL;
		rootPtr->dir_abs_path = dup_str(argv[2]);	
		gettimeofday(&sample_end, NULL);				
	}

	chdir("/tmp");	  /* this is for the output of gprof */
	double mean = 0;
	for (i=0; i < sample_times; i++)
	{	
		printf("%.2f\n", est_array[i]);
		mean += abs(est_array[i] - g_file); 
	}
	printf("mean error:%.6f", mean/sample_times/g_file);
	mean = 0;
	for (i=0; i < sample_times; i++)
	{	
		printf("%ld\n", qcost_array[i]);
		mean += qcost_array[i]; 
	}
	printf("mean query:%.6f", mean/sample_times);
	
	CleanExit (2);

	return EXIT_SUCCESS;

  /* store process working directory */
  strcpy(proc_working_dir, get_current_dir_name());

  /* initialize queue */

  clearQueue(&level_q);
  
  
  return 0;
}


int random_next(int random_bound)
{
	assert(random_bound < RAND_MAX);
	return rand() % random_bound;	
}

/* Before calling begin_estimate_from
 * the dir_node struct has been allocated for root
 */
int begin_estimate_from(struct dir_node *rootPtr)
{
    int level = 0;  /* root is in level 1 */
    int clength;
    int vlength;
    struct queueLK tempvec;
    struct dir_node *cur_dir = NULL;
	            
    /* root_dir goes to queue */
    enQueue(&level_q, rootPtr);
    

    /* if queue is not empty */
    while (emptyQueue(&level_q) != 1)    
    {
        level++;
        initQueue(&tempvec);
        /* for all dirs currently in the queue 
         * these dirs should be in the same level 
         */
        for (; emptyQueue(&level_q) != 1; )
        {
            cur_dir = outQueue(&level_q);
            /* level ordering, change directory is a big problem 
             * currently save the absolute direcotory of each folder 
             */
            fast_subdirs(cur_dir);
            qcost++;
            est_total = est_total + cur_dir->sub_file_num * cur_dir->factor;
            
            if (cur_dir->sub_dir_num > 0)
            {
                
                vlength = cur_dir->sub_dir_num;

                /* I find it hard to know whether my sub_folder has 1000 folders
                 * or not*/
                if ((level > 6)) //x && need_backtrack(cur_dir) == 0)  
		//if (qcost > 10000)
                    clength = min (vlength, max(6, floor(vlength/2)));
                else 
                    clength = vlength;
                
                /* choose clength number of folders to add to queue */
                /* need to use permutation */
                permutation(vlength);
                int i;
                for (i = 0; i < clength; i++)
                {  
                    printf("%d ", ar[i]);  
                    cur_dir->sdirStruct[ar[i]].factor *= vlength*1.0/clength;
                    enQueue(&tempvec, &cur_dir->sdirStruct[ar[i]]);
                }
            }
        }
		struct dir_node *temp;
        for (; emptyQueue(&tempvec) != 1; )
        {
            temp = outQueue(&tempvec);
            enQueue(&level_q, temp);            
        }  
    }   
}

int need_backtrack(struct dir_node *p_dir)
{
    int i;

    for (i = 0; i < p_dir->sub_dir_num; i++)
    {
        if (p_dir->sdirStruct[i].sub_dir_num == 0 &&
            p_dir->sdirStruct[i].sub_file_num >= 1000)
        return 1;
    }
    return 0;
}


double GetResult()
{
    return (est_total/sample_times);
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

int root_flag = 0;
void fast_subdirs(struct dir_node *curDirPtr) 
{
    long int  sub_dir_num = 0;
    long int  sub_file_num = 0;

    struct dirent **namelist;
    char *path;
    size_t alloc;
    int total_num;
	int used = 0;

	printf("in dir:%s\n", curDirPtr->dir_abs_path);
    if (root_flag == 0)
		{curDirPtr->factor = 1.0;
		root_flag = 1;}
	/* already stored the subdirs struct before
	 * no need to scan the dir again */
	if (curDirPtr->bool_dir_covered == 1)
	{
		/* This change dir is really important */
		already_covered++;
		chdir(path);

		return;
	}
		
	/* so we have to scan */
	newly_covered++;
    
    path = curDirPtr->dir_abs_path;
	chdir(path);	

    total_num = scandir(path, &namelist, 0, 0);

	/* root is given like the absolute path regardless of the cur_dir */
    sub_dir_num = scandir(path, &namelist, check_type, 0);


	sub_file_num = total_num - sub_dir_num;
 	alloc = sub_dir_num - 2;
	used = 0;

 	assert(alloc >= 0);

    if (alloc > 0 && !(curDirPtr->sdirStruct
			= malloc(alloc * sizeof (struct dir_node)) )) 
	{
        //goto error_close;
		printf("malloc error!\n");
		exit(-1);
    }
    
    int temp = 0;

	for (temp = 0; temp < alloc; temp++)
	{
		curDirPtr->sdirStruct[temp].sub_dir_num = 0;
		curDirPtr->sdirStruct[temp].sub_file_num = 0;
		curDirPtr->sdirStruct[temp].bool_dir_covered = 0;
	}


	/* scan the namelist */
    for (temp = 0; temp < sub_dir_num; temp++)
    {
		if ((strcmp(namelist[temp]->d_name, ".") == 0) ||
                        (strcmp(namelist[temp]->d_name, "..") == 0))
               continue;
        /* get the absolute path for sub_dirs */
        chdir(namelist[temp]->d_name);
        
   		if (!(curDirPtr->sdirStruct[used].dir_abs_path 
		       = dup_str(get_current_dir_name()))) 
		{
			printf("get name error!!!!\n");
    	}
        curDirPtr->sdirStruct[used].factor = curDirPtr->factor;
		used++;
        chdir(path);		
	}

	sub_dir_num -= 2;

	curDirPtr->sub_file_num = sub_file_num;
	curDirPtr->sub_dir_num = sub_dir_num;
	/* update bool_dir_covered info */
	curDirPtr->bool_dir_covered = 1;
}


int check_type(const struct dirent *entry)
{
    if (entry->d_type == DT_DIR)
        return 1;
    else
        return 0;
}


/************************SIMPLE MATH WORK**********************************/
int min(int a, int b)
{
    if (a < b)
        return a;
    return b;
}

int max(int a, int b)
{
    if (a > b)
        return a;
    return b;
}

void permutation(int size)
{
  int i;
  for (i=0; i < size; i++)
    ar[i] = i;
  for (i=0; i < size-1; i++)
    swap(&ar[i], &ar[Random(i,size)]);
}

void swap(int *a, int *b)
{
    int t;
    t=*a;
    *a=*b;
    *b=t;
}


int Random(int left, int right)
{
    
  return left + rand() % (right-left);
}

/******************EXIT HANDLING FUNCTION*****************************/
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
//	printf("\ndirs newly opened %ld\ndirs already_covered %ld\n",
//			newly_covered, already_covered);
//	printf("there are on average %.2f files\n", GetResult());
    puts("=============================================================");
    printf("Total Time:%ld milliseconds\n", 
	(end.tv_sec-start.tv_sec)*1000+(end.tv_usec-start.tv_usec)/1000);
    printf("Total Time:%ld seconds\n", 
	(end.tv_sec-start.tv_sec)*1+(end.tv_usec-start.tv_usec)/1000000);
	exit(0);
}


