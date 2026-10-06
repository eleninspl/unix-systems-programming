#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define DEFAULT "\033[30m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"

bool isInt(char *val) {
    for (int i = 0; val[i] != '\0'; i++) {
        if (val[i] < '0' || val[i] > '9') {
            return false;
        }
    }
    return true;
}

bool validString(int argc, char *argv[]) {
    if (argc != 2 && argc != 3) {
        return false;
    }
    if (!isInt(argv[1])) {
        return false;
    }
    if (argc == 3) {
        if (strcmp(argv[2], "--random") != 0 && strcmp(argv[2], "--round-robin") != 0) {
            return false;
        }
    }
    return true;
}

int **create_pipe(int n) {
    int **fd;
    fd = (int **)malloc(n * sizeof(int *));
    if (fd == NULL) {
        perror("malloc");
        exit(1);
    }

    for (int i = 0; i < n; i++) {
        fd[i] = (int *)malloc(2 * sizeof(int));
        if (fd[i] == NULL) {
            perror("malloc");
            exit(1);
        }
        if (pipe(fd[i]) == -1) {
            perror("pipe");
            for (int j = 0; j < i + 1; j++) {
                close(fd[j][0]);
                close(fd[j][1]);
                free(fd[j]);
            }
            free(fd);
            exit(1);
        }
    }
    return fd;
}

void free_pipe (int **fd, int n){
    for (int i = 0; i < n; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
        free(fd[i]);
    }
    free(fd);
}

void terminate_children(pid_t *child_pid, int n) {
    int status;
    for (int i = 0; i < n; i++) {
        if (kill(child_pid[i], SIGTERM) == -1) {
            perror("kill");
            exit(1);
        }
        waitpid(child_pid[i], &status, 0);
        printf(BLUE "[Parent] [%d] Child %d with PID=%d terminated successfully!" DEFAULT "\n" , getpid(), i, child_pid[i]);
    }
}

int main(int argc, char *argv[]) {
    if (!validString(argc, argv)) {
        fprintf(stderr, MAGENTA "Usage: %s <nChildren> [--random] [--round-robin]" DEFAULT "\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0) {
        fprintf(stderr, MAGENTA "Usage: %s <nChildren> [--random] [--round-robin]" DEFAULT "\n", argv[0]);
        return 0;
    }

    int n = atoi(argv[1]); 
    int selected_child = n - 1;
    bool round_robin = (argc == 2 || strcmp(argv[2], "--round-robin") == 0);
    srand((unsigned int)time(NULL));
    pid_t parent_pid = getpid();
    
    int **fd_child_to_parent, **fd_parent_to_child;
    fd_child_to_parent = create_pipe(n);
    fd_parent_to_child = create_pipe(n);
    
    pid_t *child_pid = malloc(n * sizeof(pid_t));
    if (child_pid == NULL) {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            return 1;
        }
        else if (pid == 0) {
            close(fd_child_to_parent[i][0]);
            close(fd_parent_to_child[i][1]);

            int val;
            while (1) {
                if (read(fd_parent_to_child[i][0], &val, sizeof(int)) == -1) {
                    perror("read");
                    exit(1);
                }
                printf(CYAN "[Child %d] [%d] Child received %d!" DEFAULT "\n", i, getpid(), val);
                val--;
                sleep(10);
                if (write(fd_child_to_parent[i][1], &val, sizeof(int)) == -1) {
                    perror("write");
                    exit(1);
                }
                printf(CYAN "[Child %d] [%d] Child finished hard work, writing back %d." DEFAULT "\n", i, getpid(), val);
            }
        }
        else {
        	close(fd_parent_to_child[i][0]);
            close(fd_child_to_parent[i][1]);
            child_pid[i] = pid;
        }
    }

    struct pollfd *pollfds = malloc((n+1) * sizeof(struct pollfd));
    if (pollfds == NULL) {
        perror("malloc");
        return 1;
    }

    pollfds[0].fd = fileno(stdin);
  	pollfds[0].events = POLLIN;
  	for (int i = 0; i < n; i++) {
        pollfds[i+1].fd = fd_child_to_parent[i][0];
        pollfds[i+1].events = POLLIN;
    }

    while(1){
    	if (poll(pollfds, n+1, -1) < 0) {
    		perror("poll");
    		return 1;
    	}

    	if (pollfds[0].revents & POLLIN) {

            char *input = NULL;
            size_t input_size = 0;
			if (getline(&input, &input_size, stdin) == -1) {
    			perror("getline");
    			return 1;
			}
			input[strcspn(input, "\n")] = '\0';

			if (strlen(input) == 0) {
    			continue;
			}
            else if (strcmp(input, "help") == 0) {
                printf(MAGENTA "Type a number to send job to a child!" DEFAULT "\n");
            }
            else if (strcmp(input, "exit") == 0) {
                terminate_children(child_pid, n);
                free(child_pid);
                free(input);
                free(pollfds);
                free_pipe(fd_child_to_parent, n);
                free_pipe(fd_parent_to_child, n);

                printf(BLUE "[Parent] [%d] All children exited, terminating as well." DEFAULT "\n" , parent_pid);
                return 0;
            }
            else if (!isInt(input)) {
                printf(MAGENTA "Invalid input!" DEFAULT "\n");
            }
            else {
                int val = atoi(input);
                selected_child = round_robin ? (selected_child + 1) % n : rand() % n;
                printf(BLUE "[Parent] [%d] Assigned %d to child %d." DEFAULT "\n", parent_pid, val, selected_child);
                if (write(fd_parent_to_child[selected_child][1], &val, sizeof(int)) == -1) {
                    perror("write");
                    return 1;
                }
            }
        }

        for (int i = 0; i < n; i++) {
            if (pollfds[i+1].revents & POLLIN) {
                int val;
                if (read(fd_child_to_parent[i][0], &val, sizeof(int)) == -1) {
                    perror("read");
                    return 1;
                }
                printf(BLUE "[Parent] [%d] Recieved %d from child %d." DEFAULT "\n", parent_pid, val, i); 
            }
        }
    }
}
