#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "builtins.h"
#include "config.h"
#include "siparse.h"
#include "sighandler.h"
#include "mshell.h"

volatile int fg_cnt = 0;
pid_t fg_proc[MAX_FG_PROCESSES];
struct note notes[MAX_NOTES_NO];
volatile int notes_cnt = 0;
sigset_t default_mask;
sigset_t sigchld_bloc;

int
main(int argc, char *argv[])
{
	sigemptyset(&default_mask);
	sigemptyset(&sigchld_bloc);
	sigaddset(&sigchld_bloc, SIGCHLD);
	
	setup_sighandler(SIGCHLD);
	setup_sighandler(SIGINT);

	char buf[MAX_LINE_LENGTH];
	buf[MAX_LINE_LENGTH-1] = '\0';
	char cmd_buf[MAX_CMD_LENGTH];
	int is_term = 0;
	struct stat st;

	fstat(STDIN_FILENO, &st);
	if (S_ISCHR(st.st_mode)) {
		is_term = 1;
	}
	process_inp(buf, cmd_buf, is_term);

	return 0;
}

void
fill_argv(const cmd_data* data, char** argv) {
	argseq* iter = data->cmd->args;
	for (int i = 0; i <= data->argc+1; i++) {
		if (i == data->argc) {
			argv[i] = (char*)0;
		} else {
			argv[i] = iter->arg;
			iter = iter->next;
		}
	}
}

int 
add_fg_proc(pid_t proc) {
	if (fg_cnt < MAX_FG_PROCESSES) {
		fg_proc[fg_cnt] = proc;
		fg_cnt++;
		return 0;
	} 

	return -1;
}


int 
run_pipeline(char* buff) {
	sigprocmask(SIG_BLOCK, &sigchld_bloc, NULL);
	commandseq* cmdseq;
	pipeline* ln;

	int res = parse_buf(&ln, buff);
	if (res < 0) return -1;

	cmdseq = ln->commands;

	commandseq* p = cmdseq;
	int pipe_len = 0;

	do {
		pipe_len++;
		p = p->next;
	} while (p != cmdseq);
	// forking process
	
	int fd_prev[2] = {STDIN_FILENO, -1};
	int fd_p[2];
	int fd_in, fd_out;
	int counter = 1;

	int in_fg = (ln->flags & INBACKGROUND) == 0;

	do {
		command* com = p->com;
		cmd_data data;

		parse_cmd_args(com, &data);

		int name_len = strlen(data.name);
		builtin_pair *ptr = builtins_table;

		char* argv[data.argc + 2];
		fill_argv(&data, argv);

		// check for a builtin
		// ----->
		// code down below runs only if pipe_len == 1, so the pipeline is trivial
		if (pipe_len == 1) {
			while (ptr->name != NULL) {
				if (strncmp(ptr->name, argv[0], name_len) == 0) {
					int res = ptr->fun(argv);
					return res;
				}
				ptr++;
			}
		}
		// ----->

		if (counter < pipe_len) {
			if (pipe(fd_p) != 0) {
				exit(1);
			}
			fd_out = fd_p[1];
		} else {
			fd_out = STDOUT_FILENO;
		}

		fd_in = (counter == 1) ? STDIN_FILENO : fd_prev[0];
		
		pid_t chld_pid = fork();

		if (chld_pid == 0) {			
			if (fd_in != STDIN_FILENO) {
				dup2(fd_in, STDIN_FILENO);
				close(fd_in);
			}

			if (fd_out != STDOUT_FILENO) {
				dup2(fd_out, STDOUT_FILENO);
				close(fd_out);
			}

			if (fd_prev[1] != -1) close(fd_prev[1]);
			if (counter < pipe_len) close(fd_p[0]);

			chld_proc(&data, argv, com, in_fg);
			exit(1);
		}

		if (fd_in != STDIN_FILENO) close(fd_in);
		if (fd_out != STDOUT_FILENO) close(fd_out);
		if (counter < pipe_len) {
			fd_prev[0] = fd_p[0];
			fd_prev[1] = fd_p[1];
		}

		if (in_fg) {
			add_fg_proc(chld_pid);
		}

		p = p->next;
		counter++;
	} while (p != cmdseq);

	while (fg_cnt > 0) {
		sigsuspend(&default_mask);
	}
	sigprocmask(SIG_SETMASK, &default_mask, NULL);
	
	return 0;
}

int
read_p(char* buf, int is_term) {
	sigprocmask(SIG_BLOCK, &sigchld_bloc, NULL);
	// print PROMPT and notes
	if (is_term) {
		if (notes_cnt > 0) {
			int size = notes_cnt;
			struct note temp_notes[size];
			for (int i = 0; i < notes_cnt; i++) {
				temp_notes[i] = notes[i];
			}
			notes_cnt = 0;

			sigprocmask(SIG_SETMASK, &default_mask, NULL); // unblock sigchld

			for (int i = 0; i < size; i++) {
				if (WIFEXITED(temp_notes[i].status)) {
					printf("Background process %d terminated. (exited with status %d)\n",
						temp_notes[i].chld, WEXITSTATUS(temp_notes[i].status));
				} else if (WIFSIGNALED(temp_notes[i].status)) {
					printf("Background process %d terminated. (killed by signal %d)\n",
						temp_notes[i].chld, WTERMSIG(temp_notes[i].status));
				} else {
					printf("Background process %d terminated.\n", temp_notes[i].chld);
				}
			}

			fflush(stdout);
			

		} else sigprocmask(SIG_SETMASK, &default_mask, NULL);

		printf("%s", PROMPT_STR);
		fflush(stdout);

	} else sigprocmask(SIG_SETMASK, &default_mask, NULL);

	int result;
    do {
        result = read(STDIN_FILENO, buf, MAX_LINE_LENGTH);
    } while (result == -1 && errno == EINTR); // Retry if interrupted by signal
	
	sigprocmask(SIG_BLOCK, &sigchld_bloc, NULL);
    return result;
}

int 
process_inp(char* buf, char* cmd_buf, int is_term) {
	int num_ch;
	int offset = 0;
	int cmd_len = 0;
	int left = 0;
	int res;

	while ((num_ch = read_p(buf, is_term)) > 0) {
		int beg = 0;
		char* ptr;
		int i = 0;
		while ((ptr = strpbrk(buf + beg, "\n;")) != NULL) {
			*ptr = '\0';
			i = ptr - buf;
			cmd_len += i - beg + 1;
			if (cmd_len <= MAX_LINE_LENGTH) {
				memmove(cmd_buf + offset, buf + beg, cmd_len);
				if (cmd_buf[0] != '\0' && cmd_buf[0] != '#') {
					res = run_pipeline(cmd_buf);
				}
			} else 
				fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
			beg = i + 1;
			memmove(cmd_buf, cmd_buf + beg, MAX_CMD_LENGTH - beg);
			offset = 0;
			cmd_len = 0;
		}

		left = num_ch - beg;
		cmd_len += left;
		if (left + offset > MAX_CMD_LENGTH) {
			offset = 0;
		}
		memmove(cmd_buf + offset, buf + beg, left);
		offset += left;
	}
	if (offset > 0) {
		cmd_buf[offset] = '\0';
		int res = run_pipeline(cmd_buf);
	}
	return 0;
}

void 
setup_sighandler(int sig) {
	struct sigaction sc;
	sc.sa_handler = (sig == SIGCHLD) ? sigchldhandler : siginthandler;
	sc.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sigemptyset(&sc.sa_mask);

	if (sigaction(sig, &sc, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
}

int
parse_buf(pipeline** ln, char* buf) {
	pipelineseq* temp;
	temp = parseline(buf);

	if (temp == NULL) {
		fprintf(stderr, "%s", SYNTAX_ERROR_STR);
		return -1;
	}

	*ln = temp->pipeline;
	return 0;
}

int
parse_cmd_args(command* com, cmd_data* data) {
	if (com == NULL) return -1;
	argseq* argseq = com->args;
	char* name = argseq->arg;

	int argc = 0;
	do {
		argc++;
		argseq = argseq->next;
	} while (argseq != com->args);

	data->cmd = com;
	data->name = name;
	data->argc = argc;

	return 0;
}

void
validate_errno(const char* name) {
	if (errno == ENOENT) {
		fprintf(stderr, "%s: no such file or directory\n", name);
	} else if (errno == EACCES) {
		fprintf(stderr, "%s: permission denied\n", name);
	} else {
		fprintf(stderr, "%s: exec error\n", name);
	}
}

int
check_redir(redir* r, int fd, int std) {
	if (fd == -1) {
		validate_errno(r->filename);
		return -1;
	}
	dup2(fd, std);
	close(fd);

	return 0;
}

void
chld_proc(const cmd_data* data, char** argv, command* com, int in_fg) {
	if (!in_fg) {
		setsid();
	}
	redirseq* red = com->redirs;

	if (red != NULL) {
	do {
		redir* r = red->r;

		if (IS_RIN(r->flags)) {
			int fd = open(r->filename, O_RDONLY, 0644);
			if (check_redir(r, fd, STDIN_FILENO)) return;
		} else if (IS_ROUT(r->flags) && !IS_RAPPEND(r->flags)) {
			int fd = open(r->filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
			if (check_redir(r, fd, STDOUT_FILENO)) return;
		} else {
			int fd = open(r->filename, O_WRONLY | O_APPEND | O_CREAT, 0644);
			if (check_redir(r, fd, STDOUT_FILENO)) return;
		}

		red = red->next;
	} while (red != com->redirs);
	}

	sigprocmask(SIG_SETMASK, &default_mask, NULL);
	execvp(argv[0], argv);

	validate_errno(data->name);
}
