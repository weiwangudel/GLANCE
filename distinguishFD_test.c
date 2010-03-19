#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int judge_subdirs(const struct dirent * pdirent);
int file_select(struct dirent   *entry);
int check_type(const struct dirent *entry);

main(int argc, char **argv){
    struct dirent **namelist;
    int n;

    chdir(argv[1]);
    printf("cur_dir:%s\n", get_current_dir_name());
    n = scandir(argv[1], &namelist, check_type, 0);
    if (n < 0)
        perror("scandir");
    else {
/*        while(n--) {
            printf("%s\n", namelist[n]->d_name);
            free(namelist[n]);
        }
        free(namelist);
*/
       printf("%d\n", n);
    }
}

int judge_subdirs(const struct dirent * pdirent)
{
        struct stat f_ftime;  /* distinguish files from directories */

        if(stat(pdirent->d_name, &f_ftime) != 0)
        {
                printf("stat error\n");
                return;
        }
        /* currently only deal with regular file and directory 
         * are there any other kind ?
         */
        if (S_ISREG(f_ftime.st_mode))
                return 0;
        /* record the sub direcotry names 
         * this would contain the hidden files begin with "."
         */
        else if (S_ISDIR(f_ftime.st_mode))
            return 1;

}

int file_select(struct dirent   *entry)
{
      if ((strcmp(entry->d_name, ".") == 0) ||
			(strcmp(entry->d_name, "..") == 0))
						 return 0;
				else
								return 1;
		}

int check_type(const struct dirent *entry)
{
    if (entry->d_type == DT_DIR)
	return 1;
    else 
	return 0;
}
