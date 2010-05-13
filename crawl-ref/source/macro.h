/*
 *  File:       macro.cc
 *  Summary:    Crude macro-capability
 *  Written by: Juho Snellman <jsnell@lyseo.edu.ouka.fi>
 */

#ifndef MACRO_H
#define MACRO_H

#include <deque>

#include "enum.h"

class key_recorder;
typedef std::deque<int> keyseq;

class key_recorder {
public:
    bool                  paused;
    keyseq                keys;

public:
    key_recorder();

    void add_key(int key, bool reverse = false);
    void clear();
};

class pause_all_key_recorders {
public:
    pause_all_key_recorders();
    ~pause_all_key_recorders();

private:
    std::vector<bool> prev_pause_status;
};

int getchm(int (*rgetch)() = NULL);       // keymaps applied (ie for prompts)
int getchm(KeymapContext context, int (*rgetch)() = NULL);

int getch_with_command_macros(void);  // keymaps and macros (ie for commands)

void flush_input_buffer( int reason );

void macro_add_query();
void macro_init();
void macro_save();

void macro_userfn(const char *keys, const char *registryname);

void macro_buf_add(int key,
                   bool reverse = false, bool expanded = true);
void macro_buf_add(const keyseq &actions,
                   bool reverse = false, bool expanded = true);
int macro_buf_get();

void macro_buf_add_cmd(command_type cmd, bool reverse = false);

bool is_userfunction(int key);
bool is_synthetic_key(int key);

std::string get_userfunction(int key);

void add_key_recorder(key_recorder* recorder);
void remove_key_recorder(key_recorder* recorder);

bool is_processing_macro();

int get_macro_buf_size();

///////////////////////////////////////////////////////////////
// Keybinding stuff

void init_keybindings();

command_type name_to_command(std::string name);
std::string  command_to_name(command_type cmd);

command_type  key_to_command(int key, KeymapContext context);
int           command_to_key(command_type cmd);
KeymapContext context_for_command(command_type cmd);

void bind_command_to_key(command_type cmd, int key);

std::string command_to_string(command_type cmd);
void insert_commands(std::string &desc, std::vector<command_type> cmds,
                     bool formatted = true);
void insert_commands(std::string &desc, const int first, ...);
#endif
