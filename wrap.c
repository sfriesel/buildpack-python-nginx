#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

#define exit_on_error(x, msg) if(x) { perror(msg); exit(1); }

static int shutting_down = 0;

static void handle_shutdown_signal(int sig) {
	if(!shutting_down) {
		if(sig == SIGCHLD)
			printf("something died, stopping processes\n");
		else
			printf("shutdown request, stopping processes\n");
			
		shutting_down = 1;
	}
}

int main(int argc, char *argv[]) {
	if(argc < 2) {
		fputs("Usage: nginx-wrapped <app_cmd> [<app_args>...]\n", stderr);
		exit(1);
	}

	struct sigaction sa = {
		.sa_handler = &handle_shutdown_signal, .sa_flags = SA_RESTART};
	sigemptyset(&sa.sa_mask);
	exit_on_error(sigaction(SIGCHLD, &sa, 0) == -1, "sigaction");
	exit_on_error(sigaction(SIGTERM, &sa, 0) == -1, "sigaction");

	int app_pid = fork();
	exit_on_error(app_pid == -1, "fork");
	if(!app_pid) {
		sa.sa_handler = SIG_DFL;
		exit_on_error(sigaction(SIGCHLD, &sa, 0) == -1, "sigaction");
		exit_on_error(sigaction(SIGTERM, &sa, 0) == -1, "sigaction");
		execv(argv[1], argv + 1);
		exit_on_error(1, "execv");
	}

	fputs("waiting for socket(?)...\n", stderr);

	int nginx_pid = fork();
	if(nginx_pid == -1) {
		perror("fork");
		exit(1);
	}
	if(!nginx_pid) {
		sa.sa_handler = SIG_DFL;
		exit_on_error(sigaction(SIGCHLD, &sa, 0) == -1, "sigaction");
		exit_on_error(sigaction(SIGTERM, &sa, 0) == -1, "sigaction");
		execl("./parts/nginx/start-nginx", "start-nginx");
		exit_on_error(1, "execl");
	}

	while(!shutting_down) sleep(10);

	fputs("stopping nginx\n", stderr);
	kill(nginx_pid, SIGTERM);
	fputs("stopping app\n", stderr);
	kill(app_pid, SIGTERM);

	sleep(5);

	if(waitpid(nginx_pid, NULL, WNOHANG) == 0) {
		fputs("killing ningx\n", stderr);
		kill(nginx_pid, SIGKILL);
	} else {
		fputs("nginx stopped\n", stderr);
	}
	if(waitpid(app_pid, NULL, WNOHANG) == 0) {
		fputs("killing app\n", stderr);
		kill(app_pid, SIGKILL);
	} else {
		fputs("app stopped\n", stderr);
	}
}
