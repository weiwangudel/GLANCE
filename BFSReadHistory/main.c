#include <stdio.h>
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
	system(cmd_string);
}

int load_history_run()
{

}

int maticulous_calculate()
{

}

int save_result_to_history()
{
}
