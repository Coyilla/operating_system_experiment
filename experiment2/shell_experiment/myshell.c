// myshell.c  模拟Shell程序
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#define MAX_CMD_LENGTH 1024
#define MAX_ARGS 64


#define MAX_HISTORY 20
char *history[MAX_HISTORY];
int history_count = 0;


const char *supported_commands[] = {
    "cmd1",     // 显示时间
    "cmd2",     // 计算器
    "cmd3",     // 文件信息
    "ls",       // 系统命令
    "pwd",      // 系统命令
    "date",     // 系统命令
    "help",	// 帮助
    "history",	// 历史记录
    NULL
};


void print_welcome();
int is_supported_command(const char *cmd);
int parse_command(char *cmd_line, char **args);
void execute_external_command(char **args);
void execute_internal_command(char **args);
void show_help();
void add_to_history(const char *cmd);
void show_history();


int is_supported_command(const char *cmd) {
    for (int i = 0; supported_commands[i] != NULL; i++) {
        if (strcmp(cmd, supported_commands[i]) == 0) {
	    add_to_history(cmd);
            return 1;
        }
    }
    return 0;
}


void print_welcome() {
    printf("欢迎使用模拟Shell程序\n");
    printf("支持的命令:\n");
    printf("  cmd1     - 显示当前时间\n");
    printf("  cmd2     - 简单计算器\n");
    printf("  cmd3     - 文件信息查看器\n");
    printf("  ls       - 列出目录内容\n");
    printf("  pwd      - 显示当前目录\n");
    printf("  date     - 显示系统时间\n");
    printf("  help     - 显示帮助信息\n");
    printf("  history  - 显示命令历史\n");
    printf("  exit     - 退出shell\n");
    printf("\n");
}


int parse_command(char *cmd_line, char **args) {
    int i = 0;
    char *token = strtok(cmd_line, " \t\n");
    
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL; 
    
    return i; 
}


void execute_external_command(char **args) {
    pid_t pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, "错误: 创建子进程失败\n");
        return;
    }
    
    if (pid == 0) { 

        execvp(args[0], args);
        

        fprintf(stderr, "错误: 无法执行命令 '%s'\n", args[0]);
        exit(1);
    } else { 
        int status;
        waitpid(pid, &status, 0); 
        
        if (WIFEXITED(status)) {
            printf("命令执行完毕，返回值: %d\n", WEXITSTATUS(status));
        } else {
            printf("命令异常终止\n");
        }
    }
}


void execute_internal_command(char **args) {
    if (strcmp(args[0], "cmd1") == 0) {
        char *cmd_args[] = {"./cmd1", NULL};
        execute_external_command(cmd_args);
    } 
    else if (strcmp(args[0], "cmd2") == 0) {
        char *cmd_args[] = {"./cmd2", NULL};
        execute_external_command(cmd_args);
    } 
    else if (strcmp(args[0], "cmd3") == 0) {

        char *cmd_args[MAX_ARGS];
        cmd_args[0] = "./cmd3";
        for (int i = 1; args[i] != NULL; i++) {
            cmd_args[i] = args[i];
        }
        cmd_args[(args[1] ? 2 : 1)] = NULL;
        execute_external_command(cmd_args);
    }
    else if(strcmp(args[0],"help")==0){
    	show_help();
    }
    else if(strcmp(args[0],"history")==0){
    	show_history();
    }
    else {

        execute_external_command(args);
    }
}


void show_help() {
    printf("模拟Shell 帮助\n");
    printf("内部命令:\n");
    printf("  help     - 显示此帮助信息\n");
    printf("  history  - 显示历史信息\n");
    printf("  exit     - 退出shell\n");
    printf("\n自定义命令:\n");
    printf("  cmd1     - 显示当前时间\n");
    printf("  cmd2     - 简单计算器\n");
    printf("  cmd3     - 文件信息查看器\n");
    printf("\n系统命令:\n");
    printf("  ls       - 列出目录内容\n");
    printf("  pwd      - 显示当前目录\n");
    printf("  date     - 显示系统时间\n");

}



void add_to_history(const char *cmd) {
    if (history_count < MAX_HISTORY) {
        history[history_count++] = strdup(cmd);
    }else{
    	for(int i=0;i<MAX_HISTORY-1;i++){
	    history[i]=history[i+1];
	}
	history[MAX_HISTORY-1]=strdup(cmd);
    }
}

void show_history() {
    printf("命令历史\n");
    for (int i = 0; i < history_count; i++) {
        printf("%3d: %s\n", i + 1, history[i]);
    }
}


int main() {
    char cmd_line[MAX_CMD_LENGTH];
    char *args[MAX_ARGS];
    
    print_welcome();
    
    while (1) {

        printf("myshell> ");
        fflush(stdout);
        

        if (fgets(cmd_line, MAX_CMD_LENGTH, stdin) == NULL) {
            printf("\n");
            break;
        }
        

        cmd_line[strcspn(cmd_line, "\n")] = '\0';
        

        if (cmd_line[0] == '\0') {
            continue;
        }
        

        int arg_count = parse_command(cmd_line, args);
        if (arg_count == 0) {
            continue;
        }
        

        if (strcmp(args[0], "exit") == 0) {
            printf("正在退出模拟Shell...\n");
            break;
        }
        

        if (is_supported_command(args[0])) {
            execute_internal_command(args);
        } else {
            printf("Command not found: '%s'\n", args[0]);
            printf("输入 'help' 查看支持的命令\n");
        }
        
        printf("\n");
    }
    
    printf("再见！\n");
    return 0;
}
