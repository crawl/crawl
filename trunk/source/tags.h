/*
 *  File:       tags.h
 *  Summary:    Auxilary functions to make savefile versioning simpler.
 *  Written by: Gordon Lipford
 *
 *  Change History (most recent first):
 *
 *   <1>   27 Jan 2001      GDL    Created
 */

#ifndef TAGS_H
#define TAGS_H

#include <stdio.h>
#include "externs.h"

// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files tags
 * *********************************************************************** */
int write2(FILE * file, char *buffer, unsigned int count);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files tags
 * *********************************************************************** */
int read2(FILE * file, char *buffer, unsigned int count);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files tags
 * *********************************************************************** */
void marshallByte(struct tagHeader &th, char data);
void marshallShort(struct tagHeader &th, short data);
void marshallLong(struct tagHeader &th, long data);
void marshallFloat(struct tagHeader &th, float data);
void marshallBoolean(struct tagHeader &th, bool data);
void marshallString(struct tagHeader &th, char *data, int maxSize = 0);

// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: tags files
 * *********************************************************************** */
char unmarshallByte(struct tagHeader &th);
short unmarshallShort(struct tagHeader &th);
long unmarshallLong(struct tagHeader &th);
float unmarshallFloat(struct tagHeader &th);
bool unmarshallBoolean(struct tagHeader &th);
void unmarshallString(struct tagHeader &th, char *data, int maxSize);

void make_date_string( time_t in_date, char buff[20] );
time_t parse_date_string( char[20] );

// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void tag_init(long largest_tag = 50000);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void tag_construct(struct tagHeader &th, int i);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void tag_write(struct tagHeader &th, FILE *saveFile);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void tag_set_expected(char tags[], int fileType);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
void tag_missing(int tag, char minorVersion);


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: files
 * *********************************************************************** */
int tag_read(FILE *fp, char minorVersion);

#endif // TAGS_H
