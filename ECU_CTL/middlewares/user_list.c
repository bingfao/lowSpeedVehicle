/*
 * @Author: your name
 * @Date: 2024-10-12 10:05:56
 * @LastEditTime: 2024-10-12 10:06:15
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \e_bike_ctrl_v1\Middlewares\user_list.c
 */

#include "user_list.h"

void slist_init(SLIST_NODE_t *list)
{
    list->next = NULL;
}

uint32_t slist_len(SLIST_NODE_t *slist)
{
    SLIST_NODE_t *s_node = slist;
    uint32_t len = 0;

    while (s_node->next) {
        len++;
        s_node = s_node->next;
    }

    return len;
}

void slist_instert(SLIST_NODE_t *slist, SLIST_NODE_t *node)
{
    node->next = slist->next;
    slist->next = node;
}

void slist_remove(SLIST_NODE_t *slist, SLIST_NODE_t *node)
{
    SLIST_NODE_t *s_node = slist;

    while (s_node->next) {
        if (s_node->next == node) {
            s_node->next = s_node->next->next;
            break;
        }
        s_node = s_node->next;
    }
}

void slist_add(SLIST_NODE_t *slist, SLIST_NODE_t *node)
{
    SLIST_NODE_t *s_node = slist;

    while (s_node->next) {
        s_node = s_node->next;
    }
    s_node->next = node;
    node->next = NULL;
}

SLIST_NODE_t *slist_get_end(SLIST_NODE_t *slist)
{
    SLIST_NODE_t *s_node = slist;

    while (s_node->next) {
        s_node = s_node->next;
    }

    return s_node;
}
