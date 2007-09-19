/*
 *  File:       macro.cc
 *  Summary:    Crude macro-capability
 *  Written by: Juho Snellman <jsnell@lyseo.edu.ouka.fi>
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <2>     6/25/02        JS              Removed old cruft
 *               <1>     -/--/--        JS              Created
 */

#ifndef MACRO_H
#define MACRO_H

#include <deque>

#ifndef MACRO_CC

#undef getch
#define getch() getchm()

#endif

enum KeymapContext {
    KC_DEFAULT,         // For no-arg getchm(), must be zero.
    KC_LEVELMAP,        // When in the 'X' level map
    KC_TARGETING,       // Only during 'x' and other targeting modes
    KC_CONFIRM,         // When being asked y/n/q questions

    KC_CONTEXT_COUNT    // Must always be the last
};

class key_recorder;
typedef bool (*key_recorder_callback)(key_recorder *recorder,
                                      int &ch, bool reverse);
typedef std::deque<int> keyseq;

class key_recorder {
public:
    bool                  paused;
    keyseq                keys;
    keyseq                macro_trigger_keys;
    key_recorder_callback call_back;
    void*                 call_back_data;

public:
    key_recorder(key_recorder_callback cb      = NULL,
                 void*                 cb_data = NULL);

    void add_key(int key, bool reverse = false);
    void remove_trigger_keys(int num_keys);
    void clear();
};

int getchm(int (*rgetch)() = NULL);       // keymaps applied (ie for prompts)
int getchm(KeymapContext context, int (*rgetch)() = NULL);

int getch_with_command_macros(void);  // keymaps and macros (ie for commands)

void flush_input_buffer( int reason );

void macro_add_query(void);
int macro_init(void);
void macro_save(void);

void macro_userfn(const char *keys, const char *registryname);

void macro_buf_add(int key, bool reverse = false);
void macro_buf_add(const keyseq &actions, bool reverse = false );

bool is_userfunction(int key);
bool is_synthetic_key(int key);

const char *get_userfunction(int key);

void add_key_recorder(key_recorder* recorder);
void remove_key_recorder(key_recorder* recorder);

bool is_processing_macro();

void insert_macro_into_buff(const keyseq& keys);

int get_macro_buf_size();

#endif
