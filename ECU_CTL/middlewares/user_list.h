/*
 * @Author: your name
 * @Date: 2024-10-12 10:05:56
 * @LastEditTime: 2024-10-12 10:09:23
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \e_bike_ctrl_v1\Middlewares\user_list.h
 */

#ifndef __LIST_H__
#define __LIST_H__

#include <stdint.h>
#include <stdio.h>

struct slist_node
{
    struct slist_node *next;
};
typedef struct slist_node SLIST_NODE_t;  // single list

#define SLIST_FOR_EACH(pos, head)              for (pos = (head)->next; pos != NULL; pos = (pos)->next)

#define SLIST_ELEMENT_ENTRY(prt, type, member) (type *)((char *)(prt) - (char *)&((type *)0)->member)

void slist_init(SLIST_NODE_t *slist);
uint32_t slist_len(SLIST_NODE_t *slist);
void slist_instert(SLIST_NODE_t *slist, SLIST_NODE_t *node);
void slist_remove(SLIST_NODE_t *slist, SLIST_NODE_t *node);
void slist_add(SLIST_NODE_t *slist, SLIST_NODE_t *node);
SLIST_NODE_t *slist_get_end(SLIST_NODE_t *slist);

#endif
