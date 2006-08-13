/*
 *  File:       macro.cc
 *  Summary:    Crude macro-capability
 *  Written by: Juho Snellman <jsnell@lyseo.edu.ouka.fi>
 *
 *  Change History (most recent first):
 *
 *               <2>     6/25/02        JS              Removed old cruft
 *               <1>     -/--/--        JS              Created
 */

#ifdef USE_MACROS

#ifndef MACRO_H
#define MACRO_H

#ifndef MACRO_CC

#undef getch
#define getch() getchm()

#endif

enum KeymapContext {
    KC_DEFAULT,         // For no-arg getchm().
    KC_LEVELMAP,        // When in the 'X' level map
    KC_TARGETING,       // Only during 'x' and other targeting modes

    KC_CONTEXT_COUNT    // Must always be the last
};

int getchm(int (*rgetch)() = NULL);       // keymaps applied (ie for prompts)
int getchm(KeymapContext context, int (*rgetch)() = NULL);

int getch_with_command_macros(void);  // keymaps and macros (ie for commands)

void flush_input_buffer( int reason );

void macro_add_query(void);
int macro_init(void);
void macro_save(void);

void macro_userfn(const char *keys, const char *registryname);

void macro_buf_add(int key);

bool is_userfunction(int key);

const char *get_userfunction(int key);

#endif

#else 

#define getch_with_command_macros()     getch()
#define getchm(x)                       getch()
#define flush_input_buffer(XXX)         ;
#define macro_buf_add(x)                
#define is_userfunction(x)              false
#define get_userfunction(x)             NULL
#define call_userfunction(x)            

#endif
