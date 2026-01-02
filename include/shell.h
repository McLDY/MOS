// shell.h
#ifndef SHELL_H
#define SHELL_H

#include "kernel.h"
#include "serial.h"

#define MAX_CMD_LEN 256
#define MAX_ARGS 16
#define MAX_CMDS 32
#define MAX_HISTORY 10

typedef void (*cmd_handler_t)(int argc, char *argv[]);

typedef struct {
    const char *name;
    const char *description;
    cmd_handler_t handler;
} command_t;

typedef struct {
    char input_buffer[MAX_CMD_LEN];
    uint32_t buffer_pos;

    char history[MAX_HISTORY][MAX_CMD_LEN];
    uint32_t history_count;
    uint32_t history_index;
    uint32_t history_pos;

    uint8_t escape_state;
} shell_state_t;

typedef void (*term_output_func)(const char*);

void shell_init(void);
void shell_process_char(char c);
void shell_execute_command(const char *cmd_line);
void shell_print_prompt(void);
void shell_print(const char *str);
void shell_printf(const char* fmt, ...);
void shell_set_term_output(term_output_func func);

void shell_save_to_history(const char *cmd);
void shell_show_history_up(void);
void shell_show_history_down(void);
void shell_clear_line(void);
void shell_refresh_line(void);

uint32_t shell_strtoul(const char *str, char **endptr, int base);

#endif
