/*
 * netlist.c
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

tcp_conn_t *new_tcpconn(int fd, int isconnect, 
	int port, char *host_addr, int host_port, void *extdata)
{
	struct hostent *nlp_host;
	while ((nlp_host=gethostbyname(host_addr))==NULL){
    	perror("Resolve addr Error!\n");
		return NULL;
	}

	tcp_conn_t *tconn = calloc(1, sizeof(tcp_conn_t));
	tconn->fd = fd;
	tconn->isconnect = isconnect;
	tconn->port = port;
	strcpy(tconn->host_addr, host_addr);
	tconn->host_port = host_port;
	tconn->host_in.sin_family = PF_INET;
	tconn->host_in.sin_port = htons(host_port);
	tconn->host_in.sin_addr.s_addr = 
		((struct in_addr *)(nlp_host->h_addr))->s_addr;
	tconn->extdata = extdata;
	tconn->next = NULL;

	return tconn;
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

tcp_conn_t *queryfrom_tcpconn_list_with_localport(int port)
{
	tcp_conn_t *t_list;
	
	if(tcp_conn_list.p_head != NULL)
	{
		for(t_list=tcp_conn_list.p_head; t_list!=NULL; t_list=t_list->next)
		{
			if(port == t_list->port)
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

		if(t_list)
			free(t_list->extdata);
		free(t_list);
		tcp_conn_list.num--;
		return 0;
	}

	AI_PRINTF("[%s] %s:no found connectin in tcp conn list\n",
				get_current_time(), __FUNCTION__);
	return -1;
}

void clear_all_conn(void (*del_call)(int))
{
	tcp_conn_t *t_list, *b_list;

	t_list=tcp_conn_list.p_head;
	while (t_list != NULL)
	{
		b_list = t_list;
		t_list = t_list->next;

		del_call(b_list->fd);

		free(b_list->extdata);
		free(b_list);
	}

	tcp_conn_list.p_head = NULL;
	tcp_conn_list.num = 0;
}

