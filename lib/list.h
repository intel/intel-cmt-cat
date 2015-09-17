/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2015 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
 *
 */

/**
 * @brief Double linked list managment module
 */

#ifndef __PQOS_LIST_H__
#define __PQOS_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

struct list_item {
        struct list_item *next;
        struct list_item *prev;
};

#define LIST_HEAD_INIT(declaration) {&(declaration), &(declaration)}
#define LIST_HEAD_DECLARE(declaration) \
        struct list_item declaration = LIST_HEAD_INIT(declaration)

/**
 * @brief Initializes head of the list
 * @param [in] list pointer to list_item being head of the list
 */
static inline
void list_head_init(struct list_item *list)
{
        list->next = list;
        list->prev = list;
}

/**
 * @brief Adds \a item to the list
 * @param [in] item pointer to new element of the list
 * @param [in] head pointer to head of the list
 */
static inline
void list_add(struct list_item *item,
              struct list_item *head)
{
        struct list_item *next = head->next;

        next->prev = item;
        head->next = item;
        item->next = next;
        item->prev = head;
}

/**
 * @brief Adds \a item to tail of the list
 * @param [in] item pointer to new element of the list
 * @param [in] head pointer to head of the list
 */
static inline
void list_add_tail(struct list_item *item,
                   struct list_item *head)
{
        struct list_item *prev = head->prev;

        head->prev = item;
        prev->next = item;
        item->next = head;
        item->prev = prev;
}

/**
 * @brief Removes item element from the list
 * @param [in] item list item to be removed
 */
static inline
void list_del(struct list_item *item)
{
        struct list_item *next = item->next;
        struct list_item *prev = item->prev;

        prev->next = next;
        next->prev = prev;
}

/**
 * @brief Checks if list is empty
 * @param [in] head pointer to head of the list
 * @return List empty status
 */
static inline
int list_empty(struct list_item *head)
{
        return (head->next == head);
}

/**
 * @brief Gets pointer to container object of list_item at \a ptr
 * @param [in] ptr pointer to list_item memebr of the structure
 * @param [in] type data type struct structure the list is embedded into
 * @param [in] member name list_item within \a type structure
 * @return Pointer of \a type containing \a member at \a ptr
 */
#define list_get_container(ptr, type, member)   \
        ((type *) ((char *)(ptr) - (char *)(&((type *)NULL)->member)))

/**
 * @brief Iterates over a list
 * @param pos struct list_item pointer being an iterator
 * @param aux auxiliary struct list_item pointer for temporary storage
 * @param head struct list_item pointer, head of the list
 */
#define list_for_each(pos, aux, head)             \
        for (pos = (head)->next, aux = pos->next; \
             pos != (head);                       \
             pos = aux, aux = pos->next)

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_LIST_H__ */
