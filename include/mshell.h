#ifndef MSHELL_H
#define MSHELL_H

#include <sys/types.h>
#include "config.h"
#include "siparse.h"

struct note {
	pid_t chld;
	int status;
};

extern volatile int fg_cnt;
extern pid_t fg_proc[MAX_FG_PROCESSES];
extern struct note notes[MAX_NOTES_NO];
extern volatile int notes_cnt;
extern sigset_t default_mask;
extern sigset_t sigchld_bloc;

typedef struct cmd_data {
	char* name;
	int argc;
	command* cmd;
}cmd_data;

void
fill_argv(const cmd_data* data, char** argv);

int 
add_fg_proc(pid_t proc);

int 
run_pipeline(char* buff);

int
read_p(char* buf, int is_term);

int 
process_inp(char* buf, char* cmd_buf, int is_term);

void 
setup_sighandler(int sig);

void
validate_errno(const char* name);

void
chld_proc(const cmd_data* data, char** argv, command* com, int in_fg);

int
check_redir(redir* r, int fd, int std);

int
parse_cmd_args(command* com, cmd_data* cmd);

int
parse_buf(pipeline** ln, char* buf);

#endif
