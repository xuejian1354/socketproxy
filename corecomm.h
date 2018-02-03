/*
 * corecomm.h
 *
 * Copyright (C) 2013 loongsky development.
 *
 * Sam Chen <xuejian1354@163.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __CORECOMM_H__
#define __CORECOMM_H__

#include "globals.h"

int select_init();
void select_set(int fd);
void select_wtset(int fd);
void select_clr(int fd);
void select_wtclr(int fd);
int select_listen();

#endif  //__CORECOMM_H__