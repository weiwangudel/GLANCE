/* Programmed by wei wang (wwang@gwu.edu)
 * Directed by professor Howie Huang (howie@gwu.edu)
 *
 * May 25, 2010
 *
 * Count a filesystem use level order traversing and permutation
 * But keep no history of previously traversed directories.
 * If only dealing with long int sub_file_num but not free absolute path
 * Memory usage won't go down.
 * Only free unused dir_abs_path(Didnot free subdir) 557M vs 605M on m10M
 
 * free(cur_dir)
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


#define MAX_PERMU 1000000
#define MAX_DRILL_DOWN 1000


long int g_cur_sub_file_num; /* current directory's number of files contained */
long int g_cur_sub_dir_num;  /* current directory's number of dirs contained */

/* New struct for not saving history */
struct dir_node
{
    double factor;	
	char *dir_abs_path;	 /* absolute path, needed for BFS */			
	struct dir_node *sdirStruct; /* child array dynamically allocated */
};
struct timeval start;
struct timeval end;

double est_total;
long int sample_times;
long int qcost = 0;
long int already_covered = 0;
long int newly_covered = 0;

/* program parameters */
long int g_folder;
long int g_file;
long int g_qcost_thresh;  /* smaller than this value, we crawl */
unsigned int g_level_thresh; /* int because level is not so large */
unsigned int g_sdir_thresh; /* para2: sub dir threshold */
double g_percentage; /* how large percent to randomly choose */
double g_preset_qcost;
double g_exit_thresh;

int root_flag = 0; /* only set root factor to 1 in fast_subdir */

void CleanExit(int sig);
static char *dup_str(const char *s);
int begin_estimate_from(struct dir_node *rootPtr);
int check_type(const struct dirent *entry);
void fast_subdirs(struct dir_node *curDirPtr);

/* why do I have to redefine to avoid the warning of get_current_dir_name? */
char *get_current_dir_name(void);
double floor(double);

int ar[MAX_PERMU];
void swap(int *a, int *b);
void permutation(int size);
struct queueLK level_q;

char proc_working_dir[100];
char *g_root_abs_name;

int main(int argc, char* argv[]) 
{
	long int i;
	struct dir_node *rootPtr;

	struct dir_node root_dir; /* root directory for estimation */
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

	if (argc != 11)
	{
		printf("Usage: %s \n", argv[0]);
		printf("arg 1: drill down times (must be 1 to support exit percent)\n");
		printf("arg 2: file system dir (can be relative path now)\n");
		printf("arg 3: real dirs\n");
		printf("arg 4: real file number\n");
		printf("arg 5: crawl qcost threshold\n");
		printf("arg 6: crawl level threshold\n");
		printf("if only qcost, then level = 0\n");
		printf("if only level, then qcost = 0\n");
		printf("don't use both > 0\n");
		printf("arg 7: sub_dir_num for max(sub_dir_num, **)\n");
		printf("arg 8: percentage chosen, 2 means 50 percent\n"); 
		printf("arg 9: qcost begin percentage\n");
		printf("arg 10: qcost exit percentage\n"); 
		return EXIT_FAILURE; 
	}
	if (chdir(argv[2]) != 0)
	{
		printf("Error when chdir to %s", argv[2]);
		return EXIT_FAILURE; 
	}
	
	/* this can support relative path easily */
    g_root_abs_name = dup_str(get_current_dir_name());	
	sample_times = atol(argv[1]);
	assert(sample_times <= MAX_DRILL_DOWN);

	g_folder = atol(argv[3]);
	g_file = atol(argv[4]);
	g_qcost_thresh = atol(argv[5]); 
	g_level_thresh = atoi(argv[6]);
	g_sdir_thresh = atoi(argv[7]);
	g_percentage = atof(argv[8]);
	g_preset_qcost = atof(argv[9]);
	g_exit_thresh = atof(argv[10]);

	assert(g_qcost_thresh*g_level_thresh == 0);

	/* initialize the value */
	{
        est_total = 0;
        initQueue(&level_q);
	}

	/* Initialize the dir_node struct */
	rootPtr = &root_dir;
	rootPtr->sdirStruct = NULL;
    rootPtr->dir_abs_path = dup_str(g_root_abs_name);

	int seed = (int)time(0);
	srand(seed);//different seed number for random function
    //printf("%d\t", seed);
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
		rootPtr->sdirStruct = NULL;
    	rootPtr->dir_abs_path = dup_str(g_root_abs_name);
		root_flag = 0;
		gettimeofday(&sample_end, NULL);				
	}

	chdir("/tmp");	  /* this is for the output of gprof */
	

	double mean = 0;
	//double not_abs = 0;
	for (i=0; i < sample_times; i++)
	{	
		mean += abs(est_array[i] - g_file); 
		//not_abs += est_array[i] - g_file;
	}
	mean /= sample_times;

	
    printf("%.6f\t", mean/g_file);
 
	mean = 0;
	for (i=0; i < sample_times; i++)
	{	
		mean += qcost_array[i]; 
	}
	mean /= sample_times;

	printf("%.4f\t", mean/g_folder);

  	clearQueue(&level_q);
	CleanExit (2);
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
            est_total = est_total + g_cur_sub_file_num * cur_dir->factor;

	    /* add qcost control, early exit */
            if (qcost > g_preset_qcost * g_folder)
	    	{
    			gettimeofday(&end, NULL ); 
				printf("%s\t", g_root_abs_name);
				printf("%d\t%d\t%f\t", g_level_thresh, 
							g_sdir_thresh, g_percentage);
	
				printf("%.6f\t", abs(est_total - g_file) * 1.0 / g_file);
				printf("%.6f\t", qcost*1.0 / g_folder);
				printf("%ld\n", 
				    (end.tv_sec-start.tv_sec)*1
						+(end.tv_usec-start.tv_usec)/1000000);

				g_preset_qcost += 0.05;
				if (g_preset_qcost > g_exit_thresh)
					exit(0);	
		
            }             
            
            if (g_cur_sub_dir_num > 0)
            {
                
                vlength = g_cur_sub_dir_num;

				if ((qcost > g_qcost_thresh) && level > g_level_thresh)
                    clength = min (vlength, 
						max(g_sdir_thresh, (int)floor(vlength/g_percentage)));
                else 
                    clength = vlength;
                
                /* choose clength number of folders to add to queue */
                /* need to use permutation */
				
                permutation(vlength);
                int i;				
                for (i = 0; i < clength; i++)
                {  
                    cur_dir->sdirStruct[ar[i]].factor *= vlength*1.0/clength;
                    enQueue(&tempvec, &cur_dir->sdirStruct[ar[i]]);
                }
            }
				
			/* save some memory  */
			if (1)
			{
				if (cur_dir->dir_abs_path)	
					free(cur_dir->dir_abs_path);
				if (cur_dir)
					free(cur_dir);
			}
        }
		/* there is no need to tempvec.front = NULL; 
		 * because initQueue could do that and
		 * won't affect the content that front points to
		 */
		level_q.front = tempvec.front;
		level_q.rear = tempvec.rear;
    }   
}

/* 
 * abandoned the following backtrack 
 * I find it hard to know whether my sub_folder has 1000 folders
 * or not
 * May 24, 2010
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
 */

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

void fast_subdirs(struct dir_node *curDirPtr) 
{

    struct dirent **namelist;
    char *path;
    size_t alloc;
    int total_num;
	int used = 0;

    if (root_flag == 0)
	{	
		curDirPtr->factor = 1.0;
		root_flag = 1;
	}
		
	/* so we have to scan */
	newly_covered++;
    
    path = curDirPtr->dir_abs_path;
	chdir(path);	

    total_num = scandir(path, &namelist, 0, 0);

	/* root is given like the absolute path regardless of the cur_dir */
    g_cur_sub_dir_num = scandir(path, &namelist, check_type, 0);


	g_cur_sub_file_num = total_num - g_cur_sub_dir_num;
 	alloc = g_cur_sub_dir_num - 2;
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


	/* scan the namelist */
    for (temp = 0; temp < g_cur_sub_dir_num; temp++)
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

	g_cur_sub_dir_num -= 2;

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
    
/*
    if (sig != SIGHUP)
        printf("\nExiting...\n");
    else
        printf("\nRestaring...\n");


    puts("\n\n=============================================================");
//	printf("\ndirs newly opened %ld\ndirs already_covered %ld\n",
//			newly_covered, already_covered);
    puts("=============================================================");
    printf("Total Time:%ld milliseconds\n", 
	(end.tv_sec-start.tv_sec)*1000+(end.tv_usec-start.tv_usec)/1000);
    printf("Total Time:%ld seconds\n", 
	(end.tv_sec-start.tv_sec)*1+(end.tv_usec-start.tv_usec)/1000000);
*/
	
	printf("%ld\n", 
    (end.tv_sec-start.tv_sec)*1+(end.tv_usec-start.tv_usec)/1000000);


	exit(0);
}

