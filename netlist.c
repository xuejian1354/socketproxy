/*
 * netlist.c
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

#include "netlist.h"

static tcp_conn_list_t tcp_conn_list = {
	NULL,
	0,
	TRANS_TCP_CONN_MAX_SIZE,
};

tcp_conn_list_t *get_tcp_conn_list()
{
	return &tcp_conn_list;
}

int addto_tcpconn_list(tcp_conn_t *list)
{
	tcp_conn_t *t_list;

	if(tcp_conn_list.num >= tcp_conn_list.max_size)
	{
		AI_PRINTF("[%s] %s:tcp conn list num is %d, beyond max size\n", 
			get_current_time(), __FUNCTION__, tcp_conn_list.num);

		return -1;
	}
	
	if(tcp_conn_list.p_head == NULL)
	{
		tcp_conn_list.p_head = list;
		tcp_conn_list.num = 1;
	}
	else
	{
		t_list = tcp_conn_list.p_head;
		while(t_list->next != NULL)
		{
			t_list = t_list->next;
		}
		t_list->next = list;
		tcp_conn_list.num++;
	}

	return 0;
}

tcp_conn_t *queryfrom_tcpconn_list(int fd)
{
	tcp_conn_t *t_list;
	
	if(tcp_conn_list.p_head != NULL)
	{
		for(t_list=tcp_conn_list.p_head; t_list!=NULL; t_list=t_list->next)
		{
			if(t_list->fd == fd)
			{
				return t_list;
			}
		}
	}

	AI_PRINTF("[%s] %s:no found connectin in tcp conn list\n",
				get_current_time(), __FUNCTION__);
	return NULL;
}

tcp_conn_t *queryfrom_tcpconn_list_with_ipaddr(char *ipaddr)
{
	tcp_conn_t *t_list;
	if(ipaddr == NULL)
	{
		return NULL;
	}
	
	if(tcp_conn_list.p_head != NULL)
	{
		for(t_list=tcp_conn_list.p_head; t_list!=NULL; t_list=t_list->next)
		{
			char t_ipaddr[24] = {0};
			sprintf(t_ipaddr, "%s:%u", 
				inet_ntoa(t_list->client_addr.sin_addr), 
				ntohs(t_list->client_addr.sin_port));

			if(!memcmp(t_ipaddr, ipaddr, strlen(t_ipaddr)))
			{
				return t_list;
			}
		}
	}

	AI_PRINTF("[%s] %s()%d : no found connectin in tcp conn list\n", 
		get_current_time, __FUNCTION__, __LINE__);
	return NULL;
}

int delfrom_tcpconn_list(int fd)
{
	tcp_conn_t *t_list, *b_list;
	t_list = tcp_conn_list.p_head;
	b_list = NULL;

	if(tcp_conn_list.num <= 0)
	{
		AI_PRINTF("[%s] %s:tcp conn list num is %d, no connection in list\n", 
			get_current_time(), __FUNCTION__, tcp_conn_list.num);
		
		return -1;
	}
	
	while(t_list->fd!=fd && t_list->next!=NULL)
	{
		b_list = t_list;
		t_list = t_list->next;
	}

	if(t_list->fd == fd)
	{
		if(b_list == NULL)
			tcp_conn_list.p_head = t_list->next;
		else
			b_list->next = t_list->next;

		free(t_list);
		tcp_conn_list.num--;
		return 0;
	}

	AI_PRINTF("[%s] %s:no found connectin in tcp conn list\n",
				get_current_time(), __FUNCTION__);
	return -1;
}

int delfrom_tcpconn_list_with_ipaddr(char *ipaddr)
{
	tcp_conn_t *t_list, *b_list;
	t_list = tcp_conn_list.p_head;
	b_list = NULL;
	char t_ipaddr[24] = {0};

	if(tcp_conn_list.num <= 0)
	{
		AI_PRINTF("[%s] %s:tcp conn list num is %d, no connection in list\n", 
			get_current_time() ,__FUNCTION__, tcp_conn_list.num);
		return -1;
	}

	if(ipaddr == NULL)
	{
		AI_PRINTF("[%s] %s:ipaddr error\n", get_current_time() ,__FUNCTION__);
		return -1;
	}
	
	if(tcp_conn_list.p_head != NULL)
	{
		sprintf(t_ipaddr, "%s:%u", 
			inet_ntoa(t_list->client_addr.sin_addr), 
			ntohs(t_list->client_addr.sin_port));

		while (memcmp(t_ipaddr, ipaddr, strlen(t_ipaddr)) && t_list->next!=NULL)
		{
			b_list = t_list;
			t_list=t_list->next;

			memset(t_ipaddr, 0, sizeof(t_ipaddr));
			sprintf(t_ipaddr, "%s:%u", 
				inet_ntoa(t_list->client_addr.sin_addr), 
				ntohs(t_list->client_addr.sin_port));
		}
	}

	if(!memcmp(t_ipaddr, ipaddr, strlen(t_ipaddr)))
	{
		if(b_list == NULL)
			tcp_conn_list.p_head = t_list->next;
		else
			b_list->next = t_list->next;

		free(t_list);
		tcp_conn_list.num--;
		return 0;
	}

	AI_PRINTF("[%s] %s:no found connectin in tcp conn list\n",
				get_current_time(), __FUNCTION__);
	return -1;
}

