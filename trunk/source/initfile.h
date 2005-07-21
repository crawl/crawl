/*
 *  File:       initfile.h
 *  Summary:    Simple reading of init file.
 *  Written by: David Loewenstern
 *
 *  Change History (most recent first):
 *
 *               <1>     6/9/99        DML             Created
 */


#ifndef INITFILE_H
#define INITFILE_H

#include <string>

std::string & trim_string( std::string &str );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void read_init_file(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void get_system_environment(void);


// last updated 16feb2001 {gdl}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool parse_args(int argc, char **argv, bool rc_only);


#endif
