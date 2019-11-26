
/**
 * Shell written in c
 */

#define _GNU_SOURCE
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "format.h"
#include "shell.h"
#include "vector.h"

#define CMDS_SIZE 50

static pid_t foreground = 0;

typedef struct process {
    char *command;
    pid_t pid;
    struct process *next;
} process;

static process *head = NULL;

void handler() {
    if (foreground > 0) {
        kill(foreground, SIGINT);
    }
}

void get_proc_info(pid_t pid, process_info *pi) {
    char filename[100];
    sprintf(filename, "/proc/%d/stat", pid);
    FILE *f = fopen(filename, "r");
    unsigned long int v_size;
    int dummy;
    unsigned long int dummylu;
    long int dummyl;
    char dummystr[100];
    long unsigned int utime;
    long unsigned int stime;
    long long unsigned int start_time;
    fscanf(f, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu %lu",
           &(pi->pid),
           dummystr,
           &(pi->state),
           &dummy,
           &dummy, // 5
           &dummy,
           &dummy,
           &dummy,
           &dummy,
           &dummylu, // 10
           &dummylu,
           &dummylu,
           &dummylu,
           &utime,
           &stime, // 15
           &dummyl,
           &dummyl,
           &dummyl,
           &dummyl,
           &(pi->nthreads), // 20
           &dummyl,
           &start_time,
           &v_size
           );
    fclose(f);
    pi->vsize = v_size / 1024;
    char start_buf[50];
    char time_buf[50];
    long unsigned int time_run = (utime + stime) / sysconf(_SC_CLK_TCK);
    struct tm *tm_info;
    f = fopen("/proc/stat", "r");
    char *btime_str = NULL;
    size_t btime_str_len = 0;
    for (int i = 0; i < 100; i++) {
        getline(&btime_str, &btime_str_len, f);
        if (btime_str[0] == 'b') break;
    }
    char * btime_atoi = btime_str + 6;
    int btime = atoi(btime_atoi);
    time_t start_seconds = (time_t)(btime + (int)(start_time / sysconf(_SC_CLK_TCK)));
    tm_info = localtime(&start_seconds);
    time_struct_to_string(start_buf, 50, tm_info);
    execution_time_to_string(time_buf, 50, (size_t)(time_run / 60), (size_t)(time_run % 60));

    pi->time_str = strdup(time_buf);
    pi->start_str = strdup(start_buf);
    fclose(f);
    free(btime_str);
}

void handler2(int sig) {
    (void)sig;
    pid_t pid;
    while ((pid = waitpid((pid_t)(-1), 0, WNOHANG)) > 0) {
        process *temp = head;
        while (temp->next != NULL) {
            if (temp->next->pid == pid) {
                process *freed = temp->next;
                temp->next = freed->next;
                free(freed->command);
                free(freed);
                break;
            }
            temp = temp->next;
        }
    }

}

void cmd_change(char *line, char **cmds) {
    char *str = strdup(line);
    for (int i = 0; i < CMDS_SIZE; i++) {
        if (cmds[i] != NULL) free(cmds[i]);
        cmds[i] = NULL;
    }
    char *temp = strtok(str, " ");
    int i = 0;
    for (; temp != NULL; i++) {
        cmds[i] = strdup(temp);
        temp = strtok(NULL, " ");
    }
    free(str);
    return;
}


int shell(int argc, char *argv[]) {
    // TODO: This is the entry point for your shell.
    signal(SIGINT, handler);
    signal(SIGCHLD, handler2);
    head = malloc(sizeof(process));
    head->command = strdup("./shell");
    head->pid = getpid();
    head->next = NULL;
    int file_i = 0;
    int file_count = 0;
    int hist_i = 0;
    int hist_len = 0;
    vector *fvec = NULL;
    //vector *hvec = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0) file_i = i;
        if (strcmp(argv[i], "-h") == 0) hist_i = i;
    }
    char *file = NULL;
    char *history = NULL;
    char *full_history = NULL;
    if (file_i) {
        file = strdup(argv[file_i+1]);
        FILE *f = fopen(file, "r");
        if (f == NULL) { print_script_file_error(); exit(0); }
        fvec = string_vector_create();
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, f)) != -1) {
            if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = '\0';
            vector_push_back(fvec, line);
        }
        if (line) free(line);
        fclose(f);
    }

    vector *hist = string_vector_create();

    if (hist_i) {
        history = strdup(argv[hist_i+1]);
        full_history = get_full_path(history);
        FILE *h = fopen(history, "r");
        if (h) {
            char *line = NULL;
            size_t len = 0;
            ssize_t read;
            while ((read = getline(&line, &len, h)) != -1) {
                if (line[strlen(line)-1] == '\n') line[strlen(line)-1] = '\0';
                vector_push_back(hist, line);
                hist_len++;
            }
            if (line) free(line);
            fclose(h);
        }
        else { print_history_file_error(); }
    }

    while (1) {
        char *cwd = get_current_dir_name();
        if (!file_i) {
            print_prompt(cwd, getpid());
        }
        char *line = NULL;
        size_t len = 0;
        int log_and = 0;
        int log_or = 0;
        int log_semi = 0;
        if (!file_i) {
            int read = getline(&line, &len, stdin);
            if (read == -1) {
                free(cwd);
                if (fvec) vector_destroy(fvec);
                if (hist) vector_destroy(hist);
                if (history) free(history);
                if (full_history) free(full_history);
                if (file) free(file);
                if (line) free(line);
                exit(0);
            }
        } else {
            if (file_count < (int)vector_size(fvec)) {
                print_prompt(cwd, getpid());
                line = strdup((char*)vector_get(fvec, file_count));
                file_count++;
                print_command(line);
            } else {
                if (hist_i) {
                    FILE *h = fopen(history, "a");
                    for (int i = hist_len; i < (int)vector_size(hist); i++) {
                        fprintf(h, "%s\n", (char*)vector_get(hist, i));
                    }
                    fclose(h);
                    free(history);
                    free(full_history);
                }
            vector_destroy(fvec);
            vector_destroy(hist);
            free(file);
            free(cwd);
            exit(0);
            }
        }

        // Check for built in functions
        char *line_dup = strdup(line);
        if (line_dup[strlen(line_dup)-1] == '\n') line_dup[strlen(line_dup)-1] = '\0';
        char *cmds[CMDS_SIZE];
        char **cmds2 = NULL;
        for (int i = 0; i < CMDS_SIZE; i++) {
            cmds[i] = NULL;
        }
        char *temp = strtok(line, " ");
        if (temp[strlen(temp)-1] == '\n') { temp[strlen(temp)-1] = '\0'; }
        if (strcmp(temp, "exit") == 0) {
            // if there is a history file to update, do that here
            if (hist_i) {
                FILE *h = fopen(full_history, "a");
                for (int i = hist_len; i < (int)vector_size(hist); i++) {
                    fprintf(h, "%s\n", (char*)vector_get(hist, i));
                }
                fclose(h);
                free(history);
                free(full_history);
            }
            if (file) { free(file); vector_destroy(fvec); }
            if (line) free(line);
            if (line_dup) free(line_dup);
            vector_destroy(hist);
            free(cwd);
            exit(0);
        }
        int cmds_len = 0;
        for(; temp != NULL; cmds_len++) {
            if (strcmp(temp, "&&") == 0) { log_and = 1; cmds[cmds_len] = NULL; cmds2 = cmds + cmds_len + 1; }
            else if (strcmp(temp, "||") == 0) { log_or = 1; cmds[cmds_len] = NULL; cmds2 = cmds + cmds_len + 1; }
            else if (strcmp(temp, ";") == 0) { log_semi = 1; cmds[cmds_len] = NULL; cmds2 = cmds + cmds_len + 1; }
            else if (temp[strlen(temp)-1] == ';') {
                log_semi = 1;
                temp[strlen(temp)-1] = '\0';
                cmds[cmds_len] = strdup(temp);
                cmds_len++;
                cmds[cmds_len] = NULL;
                cmds2 = cmds + cmds_len + 1;
            }
            else { cmds[cmds_len] = strdup(temp); }
            temp = strtok(NULL, " ");
        }
        if (cmds[cmds_len-1][strlen(cmds[cmds_len-1])-1] == '\n') { cmds[cmds_len-1][strlen(cmds[cmds_len-1])-1] = '\0'; }

        fflush(stdout);

        if (strcmp(cmds[0], "!history") == 0) {
            size_t vsize = vector_size(hist);
            for (size_t i = 0; i < vsize; i++) {
                print_history_line(i, ((char*)vector_get(hist, i)));
            }
            free(cwd);
            free(line);
            free(line_dup);
            for (int i = 0; i < CMDS_SIZE; i++) {
                if (cmds[i] != NULL) free(cmds[i]);
            }
            continue;
        }
        if (cmds[0][0] == '#') {
            char *num = cmds[0] + 1;
            int index = atoi(num);
            if (index >= (int)vector_size(hist)) {
                print_invalid_index();
                continue;
            }
            print_command((char*)vector_get(hist, index));
            cmd_change((char*)vector_get(hist, index), cmds);
            free(line_dup);
            line_dup = strdup((char*)vector_get(hist, index));
        }
        if (cmds[0][0] == '!') {
            char *cmd = cmds[0] + 1;
            size_t vsize = vector_size(hist);
            if (strlen(cmd) == 0) {
                print_command((char*)vector_get(hist, vsize-1));
                cmd_change((char*)vector_get(hist, vsize-1), cmds);
                free(line_dup);
                line_dup = strdup((char*)vector_get(hist, vsize-1));
            }
            int match = 0;
            for (size_t i = vsize-1; (int)i > -1; i--) {
                if (strncmp((char*)vector_get(hist, i), cmd, strlen(cmd)) == 0) {
                    match = 1;
                    print_command((char*)vector_get(hist, i));
                    cmd_change((char*)vector_get(hist, i), cmds);
                    free(line_dup);
                    line_dup = strdup((char*)vector_get(hist, i));
                    break;
                }
            }
            if (!match) {
                print_no_history_match();
            }
        }
        if (strcmp(cmds[0], "cd") == 0) {
            vector_push_back(hist, line_dup);
            free(line_dup);
            if (cmds[1] == NULL) {
                print_no_directory("");
                free(cwd);
                if (line) free(line);
                free(cmds[0]);
                continue;
            }
            int status = chdir(cmds[1]);
            if (status == -1 ) {
                print_no_directory(cmds[1]);
            }
            free(cwd);
            if (line) free(line);
            for (int i = 0; i < CMDS_SIZE; i++) {
                if (cmds[i] != NULL) free(cmds[i]);
            }
            continue;
        }
        if (strcmp(cmds[0], "stop") == 0) {
            vector_push_back(hist, line_dup);
            if (cmds[1] == NULL) {
                print_invalid_command(line_dup);
                free(cwd);
                free(cmds[0]);
                if (line) free(line);
                continue;
            }
            pid_t pid = (pid_t)atoi(cmds[1]);
            int check = kill(pid, SIGTSTP);
            if (check == -1) { print_no_process_found((int)pid); }
            else {
                process *temp = head;
                while (temp != NULL) {
                    if (temp->pid == pid) break;
                    temp = temp->next;
                }
                if (temp == NULL) continue;
                print_stopped_process((int)pid, temp->command);
            }
            free(cwd);
            free(line_dup);
            if(line) free(line);
            for (int i = 0; i < CMDS_SIZE; i++) { if (cmds[i] != NULL) free(cmds[i]); }
            continue;
        }
        if (strcmp(cmds[0], "cont") == 0) {
            vector_push_back(hist, line_dup);
            if (cmds[1] == NULL) {
                print_invalid_command(line_dup);
                free(cwd);
                free(cmds[0]);
                if (line) free(line);
                continue;
            }
            pid_t pid = (pid_t)atoi(cmds[1]);
            int check = kill(pid, SIGCONT);
            if (check == -1) { print_no_process_found((int)pid); }
            else {  }
            free(cwd);
            free(line_dup);
            if(line) free(line);
            for (int i = 0; i < CMDS_SIZE; i++) { if (cmds[i] != NULL) free(cmds[i]); }
            continue;
        }
        if (strcmp(cmds[0], "pfd") == 0) {
            vector_push_back(hist, line_dup);
            if (cmds[1] == NULL) {
                print_invalid_command(line_dup);
                free(cwd);
                free(cmds[0]);
                if(line) free(line);
                continue;
            }
            print_process_fd_info_header();
            pid_t pid = (pid_t)atoi(cmds[1]);
            char dirname[50];
            sprintf(dirname, "/proc/%d/fdinfo", pid);
            DIR *d = opendir(dirname);
            struct dirent *dp;
            dp = readdir(d);
            dp = readdir(d);
            while((dp = readdir(d)) != NULL) {
                char filename[50];
                sprintf(filename, "/proc/%d/fd/%s", pid, dp->d_name);
                char path[50];
                for (int i = 0; i < 50; i++) { path[i] = '\0'; }
                ssize_t bread = readlink(filename, path, 50);
                path[bread] = '\0';
                if (strlen(path) < 3) continue;
                sprintf(filename, "/proc/%d/fdinfo/%s", pid, dp->d_name);
                FILE *f = fopen(filename, "r");
                char *fline = NULL;
                size_t fline_l = 0;
                getline(&fline, &fline_l, f);
                char *fnum = fline;
                for (size_t i = 0; i < fline_l; i++) {
                    fnum = fnum + 1;
                    if (isdigit(fnum[0])) break;
                }
                size_t pos = (size_t)atoi(fnum);
                size_t no = (size_t)atoi(dp->d_name);
                free(fline);
                fclose(f);
                print_process_fd_info(no, pos, path);
            }
            free(cwd);
            free(line_dup);
            if(line) free(line);
            for (int i = 0; i < CMDS_SIZE; i++) { if (cmds[i] != NULL) free(cmds[i]); }
            continue;
        }
        if (strcmp(cmds[0], "ps") == 0) {
            vector_push_back(hist, line_dup);
            print_process_info_header();
            process *temp = head;
            while (temp != NULL) {
                process_info *pi = malloc(sizeof(process_info));
                get_proc_info(temp->pid, pi);  // user helper function
                pi->command = temp->command;
                print_process_info(pi);
                free(pi->time_str);
                free(pi->start_str);
                free(pi);
                temp = temp->next;
            }
            free(cwd);
            free(line_dup);
            if(line) free(line);
            for (int i = 0; i < CMDS_SIZE; i++) { if (cmds[i] != NULL) free(cmds[i]); }
            continue;
        }

        // External command:
        vector_push_back(hist, line_dup);
        //free(line_dup);

        int amp = 0;
        if (cmds[cmds_len - 1][strlen(cmds[cmds_len - 1]) - 1] == '&') {
            amp = 1;
            if (strlen(cmds[cmds_len - 1]) == 1) { free(cmds[cmds_len - 1]); cmds[cmds_len - 1] = NULL; }
            else cmds[cmds_len -1][strlen(cmds[cmds_len - 1]) - 1] = '\0';
        }
        if (!amp) free(line_dup);

        pid_t child = fork();

        if (child == -1) {
            // Error, failed to fork
            print_fork_failed();
        }
        else if (child > 0) {
            // This is the parent; wait for the child
            foreground = child;
            if (amp) {
                if (setpgid(child, child) == -1) {
                    print_setpgid_failed();
                    exit(1);
                }
                process *temp = head;
                while(temp->next != NULL) { temp = temp->next; }
                temp->next = malloc(sizeof(process));
                temp->next->command = strdup(line_dup);
                free(line_dup);
                temp->next->pid = child;
                temp->next->next = NULL;
            }
            if (!amp) {
                int status;
                pid_t no = waitpid(child, &status, 0);
                if (no == -1) {
                    print_wait_failed();
                    exit(1);
                }
                foreground = 0;
                if (log_and && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    pid_t child2 = fork();

                    if (child2 == -1) {
                        print_fork_failed();
                    }
                    else if (child2 > 0) {
                        foreground = child2;
                        int status2;
                        pid_t no2 = waitpid(child2, &status2, 0);
                        if (no2 == -1) {
                            print_wait_failed();
                            exit(1);
                        }
                        foreground = 0;
                    }
                    else {
                        if (cmds2[0][0] == '/') {
                            print_command_executed(getpid());
                            execv(cmds2[0], cmds2);
                        } else {
                        print_command_executed(getpid());
                        execvp(cmds2[0], cmds2);
                        }
                    }
                }
                else if (log_or && WIFEXITED(status) && WEXITSTATUS(status)) {
                    pid_t child2 = fork();

                    if (child2 == -1) {
                        print_fork_failed();
                    }
                    else if (child2 > 0) {
                        foreground = child2;
                        int status2;
                        pid_t no2 = waitpid(child2, &status2, 0);
                        if (no2 == -1) {
                            print_wait_failed();
                            exit(1);
                        }
                        foreground = 0;
                    }
                    else {
                        if (cmds2[0][0] == '/') {
                            print_command_executed(getpid());
                            execv(cmds2[0], cmds2);
                        } else {
                        print_command_executed(getpid());
                        execvp(cmds2[0], cmds2);
                        }
                    }

                }
                else if (log_semi && WIFEXITED(status)) {
                    pid_t child2 = fork();

                    if (child2 == -1) {
                        print_fork_failed();
                    }
                    else if (child2 > 0) {
                        foreground = child2;
                        int status2;
                        pid_t no2 = waitpid(child2, &status2, 0);
                        if (no2 == -1) {
                            print_wait_failed();
                            exit(1);
                        }
                        foreground = 0;
                    }
                    else {
                        if (cmds2[0][0] == '/') {
                            print_command_executed(getpid());
                            execv(cmds2[0], cmds2);
                        } else {
                        print_command_executed(getpid());
                        execvp(cmds2[0], cmds2);
                        }
                    }
                }
            }
        }
        else {
            // This is the child
            //free(cwd);
            //free(line);
            if (cmds[0][0] == '/') {
                print_command_executed(getpid());
                execv(cmds[0], cmds);
            } else {
            print_command_executed(getpid());
            execvp(cmds[0], cmds);
            }
            // Failed to exec if child reaches here
            print_exec_failed(cmds[0]);
            if (file) { free(file); vector_destroy(fvec); }
            if (history) free(history);
            if (full_history) free(full_history);
            if (cwd) free(cwd);
            if (line) free(line);
            vector_destroy(hist);
            for (int i = 0; i < CMDS_SIZE; i++) {
                if (cmds[i] != NULL) free(cmds[i]);
            }
            exit(1);
        }
        for (int i = 0; i < CMDS_SIZE; i++) {
            if (cmds[i] != NULL) free(cmds[i]);
        }
        //if (file != NULL) free(file);
        //if (history != NULL) free(history);
        free(cwd);
        free(line);
    }
    return 0;
}
