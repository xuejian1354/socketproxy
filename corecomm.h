/*
 * corecomm.h
 */
#ifndef __CORECOMM_H__
#define __CORECOMM_H__

#include "globals.h"
#include "nethandler.h"

int select_init();
int select_set_with_index(int index, int fd);
int select_set(int fd);
int select_wtset_with_index(int index, int fd);
int select_wtset(int fd);
void select_clr(int index, int fd);
void select_wtclr(int index, int fd);
int select_listen(int index);
int pthread_with_select();

#endif  //__CORECOMM_H__