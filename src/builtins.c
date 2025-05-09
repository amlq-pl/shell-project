#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include "builtins.h"

int echo(char*[]);
int undefined(char *[]);
int fexit(char*[]);
int fcd(char*[]);
int cd(char*[]);
int fkill(char*[]);
int fls(char*[]);

builtin_pair builtins_table[]={
	{"exit",	&fexit},
	{"lecho",	&echo},
	{"lcd",		&fcd},
	{"lkill",	&fkill},
	{"lls",		&fls},
	{"cd",		&cd},
	{NULL,NULL}
};

int 
echo( char * argv[])
{
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int
fexit(char* argv[]) {
	exit(0);
}

int
fcd(char* argv[]) {
	int argc = 0;
	int res;
	char **ptr = argv;
	while (*ptr != NULL) {
		ptr++; argc++;
	}

	if (argv[1] != NULL)
		res = chdir(argv[1]);
	else 
		res = chdir(getenv("HOME"));
	if (res < 0 || argc > 2) {
		fprintf(stderr, "Builtin lcd error.\n");
		return -1;
	}
	return res;
}

int
cd(char* argv[]) {
	int argc = 0;
	int res;
	char **ptr = argv;
	while (*ptr != NULL) {
		ptr++; argc++;
	}

	if (argv[1] != NULL)
		res = chdir(argv[1]);
	else 
		res = chdir(getenv("HOME"));
	if (res < 0 || argc > 2) {
		fprintf(stderr, "Builtin lcd error.\n");
		return -1;
	}
	return res;
}

int 
fkill(char* argv[]) {
	int argc = 0;
	int res;
	char **ptr = argv;
	while (*ptr != NULL) {
		ptr++; argc++;
	}

	if (argc == 2) {
		pid_t pid = (pid_t)strtol(argv[1], NULL, 10);
		errno = 0; // should be set according to documentation
		if (errno == EINVAL) {
			fprintf(stderr, "Conversion error.\n");
		} else if (errno == ERANGE) {
			fprintf(stderr, "Range error.\n");
		}
		res = kill(pid, SIGTERM);
	} else if (argc == 3) {
		int sig = abs((int)strtol(argv[1], NULL, 10));
		errno = 0; // should be set according to documentation
		if (errno == EINVAL) {
			fprintf(stderr, "Conversion error.\n");
		} else if (errno == ERANGE) {
			fprintf(stderr, "Range error.\n");
		}
		pid_t pid = (pid_t)strtol(argv[2], NULL, 10);
		errno = 0; // should be set according to documentation
		if (errno == EINVAL) {
			fprintf(stderr, "Conversion error.\n");
		} else if (errno == ERANGE) {
			fprintf(stderr, "Range error.\n");
		}
		res = kill(pid, sig);
	} else res = -1;

	if (res < 0) {
		fprintf(stderr, "Builtin lkill error.\n");
		return -1;
	}

	return 0;
}

int 
fls(char* argv[]) {
	const int MAX_SIZE = 1024;
	char buf[MAX_SIZE];
	char* succ;

	succ = getcwd(buf, MAX_SIZE);
	if (succ == NULL) {
		fprintf(stderr, "Builtin lls error.\n");
		return -1;
	}

	DIR* dir = opendir(succ);	

	if (dir == NULL) {
		fprintf(stderr, "Builtin lls error.\n");
		return -1;
	}

	struct dirent* ent;

	if (ent == NULL) {
		fprintf(stderr, "Builtin lls error.\n");
		return -1;
	}
	int i = 0;
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.') continue;
		printf("%s\n", ent->d_name);
		if (i == 0) fflush(stdout);
	}
	fflush(stdout);
	closedir(dir);

	return 0;
}

int 
undefined(char * argv[])
{
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}
