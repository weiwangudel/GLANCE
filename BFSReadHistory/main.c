#include <stdio.h>

#define  MAX_HIS 100

double g_his_res[MAX_HIS];
double g_cur_res;  /* the result of current run (got from pipe) */
double g_final_res;  /* the result of calculation w/history considered */
int    g_num_history;

int main(int argc, char* argv[])
{
	char cmd_string[200];
	
	prepare_cmd_string(cmd_string, argc, argv);

	read_current_run(cmd_string);

	load_history_run();
	maticulous_calculate();
	save_result_to_history();
}

int prepare_cmd_string(char *cmd_string, int argc, char **argv)
{
	
	strcpy(cmd_string, "./count 1 /home/fs/fsgen/64141_notime 97170 1055126 0 5 3 2 0 1");
}

int	read_current_run(char *cmd_string)
{
	FILE* p_count;
	char fs_buffer[100];

	if ((p_count = popen(cmd_string, "r")) == NULL)
	{
		printf("popen error!\n");
		return -1;
	}
	while (feof(p_count) == 0)
	{
		fgets(fs_buffer, 100, p_count);
		sscanf(fs_buffer, "%lf\n", &g_cur_res);
	}
	pclose(p_count);
}

int load_history_run()
{
	char read_buf[100];
	int  history_i;
	FILE* fhistory;
	int read = 0;

    if ((fhistory = fopen("history.txt", "rw+")) == NULL)
	{
		printf("fopen error!\n");
	} 

    history_i = 0;
	while (feof(fhistory) == 0)
    {	
		fscanf(fhistory, "%lf",  &g_his_res[history_i % MAX_HIS]);
		history_i++;		
	}		
	g_num_history = history_i - 1;
	/* append and close */
	fclose(fhistory);

}

int maticulous_calculate()
{
	int i = 0;
	g_final_res = g_cur_res; /* save this one but report a different? */
	double mean = 0;
	
	for (i=0; i < g_num_history; i++)
	{
		mean += g_his_res[i];
	}
	mean /= g_num_history;
        printf("\nFinal Report= 0.8*%.6f + 0.2*%.6f = %.6f\n", mean,
		g_cur_res, mean*0.8+g_cur_res*0.2);	
}

/* Append mode write to a file 
 * One result per line 
 */
int save_result_to_history()
{
	char write_buf[80];
	
	FILE* fhistory;
    if ((fhistory = fopen("history.txt", "a")) == NULL)
	{
		printf("fopen error!\n");
	} 		
	sprintf(write_buf, "%.6f\n", g_final_res);

	/* append and close */
	fwrite(write_buf, strlen(write_buf), 1, fhistory);
	fclose(fhistory);
}
