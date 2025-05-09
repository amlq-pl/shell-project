#include "sighandler.h"
#include "mshell.h"
#include <sys/types.h>
#include <sys/wait.h>

void sigchldhandler(int sig) {
	pid_t ch;
	int status;
	while ((ch = waitpid(-1, &status, WNOHANG)) > 0) {
		int pos = 0;

		if (fg_cnt == 0) pos = -1;
		else {
			int i = 0;
			while (i < fg_cnt && fg_proc[i] != ch) i++;

			if (i == fg_cnt) pos = -1;
			else pos = i;
		}
		if (pos != -1) {
			// remove proc
			fg_proc[pos] = fg_proc[fg_cnt - 1];
			fg_cnt--;
		} else {
			// make note
			struct note new_note = {ch, status};
			if (notes_cnt < MAX_NOTES_NO) {
				notes[notes_cnt] = new_note;
				notes_cnt++;
			} else {
				notes[0] = new_note;
			}
		}
	}
}

void siginthandler(int sig) {

}
