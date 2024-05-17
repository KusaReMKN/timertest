#include <sys/select.h>

#include <err.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define READSIZE	(2048)

static sigjmp_buf env;
static struct termios prev;

/*
 * Signal handler
 */
void
handler(int sig)
{
	siglongjmp(env, sig);
}

/*
 * Termination handler
 */
void
quit(void)
{
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &prev);
}

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	struct sigevent sigev;
	timer_t timerid;
	struct termios raw;
	fd_set rfds;
	volatile size_t sumread;
	ssize_t nread;
	char buf[READSIZE];
	struct itimerspec it;

	/* Get current terminal mode */
	if (isatty(STDIN_FILENO) == -1)
		err(EXIT_FAILURE, "isatty");
	if (tcgetattr(STDIN_FILENO, &prev) == -1)
		err(EXIT_FAILURE, "tcgetattr");
	atexit(quit);

	/* Jumping from signal handlers */
	switch (sigsetjmp(env, 1)) {
	case SIGINT:
		goto quit;
	case SIGUSR1:
		printf("Read %zu bytes\r\n", sumread);
		goto next;
	}

	/* Set signal handlers */
	sa.sa_handler = handler;
	sa.sa_flags = SA_RESTART;
	(void)sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) == -1)
		err(EXIT_FAILURE, "sigaction: sig=%d", SIGINT);
	if (sigaction(SIGUSR1, &sa, NULL) == -1)
		err(EXIT_FAILURE, "sigaction: sig=%d", SIGUSR1);

	/* Create a new timer */
	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = SIGUSR1;
	if (timer_create(CLOCK_MONOTONIC, &sigev, &timerid) == -1)
		err(EXIT_FAILURE, "timer_create");

	/* Set the terminal to raw mode */
	(void)memcpy(&raw, &prev, sizeof(raw));
	cfmakeraw(&raw);
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		err(EXIT_FAILURE, "tcsetattr");
	/*
	 * In practice, the terminal mode should be again to check the value,
	 * but this time it has been omitted.
	 */

next:
	sumread = 0;
	while (/* CONSTCOND */ 1) {
		/* Wait for input */
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO, &rfds);
		if (select(STDIN_FILENO+1, &rfds, NULL, NULL, NULL) == -1)
			err(EXIT_FAILURE, "select");

		/* Read input */
		if ((nread = read(STDIN_FILENO, buf, sizeof(buf))) == -1)
			err(EXIT_FAILURE, "stdin");
		sumread += nread;
		if (buf[0] == '\x03')
			raise(SIGINT);

		/* Arm the timer */
		it.it_interval.tv_sec = 0;
		it.it_interval.tv_nsec = 0;
		it.it_value.tv_sec = 1;
		it.it_value.tv_nsec = 0;
		if (timer_settime(timerid, 0, &it, NULL) == -1)
			err(EXIT_FAILURE, "timer_settime");
	}

quit:
	return 0;
}
