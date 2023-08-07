/*
 * comp2017 - assignment 3
 * dan nguyen
 * kngu7458
 */

#include "pe_exchange.h"

void penqueue(struct pidqueue *pqueue, pid_t pid)
{
	if (pqueue->size == 0) {
		pqueue->tail = malloc(sizeof(struct pidnode));
		pqueue->tail->pid = pid;
		pqueue->tail->next = NULL;
		pqueue->head = pqueue->tail;
	} else {
		pqueue->tail->next = malloc(sizeof(struct pidnode));
		pqueue->tail->next->pid = pid;
		pqueue->tail->next->next = NULL;
		pqueue->tail = pqueue->tail->next;
	}
	pqueue->size++;
}

pid_t pdequeue(struct pidqueue *pqueue)
{
	if (pqueue->size == 0)
		return (pid_t)-1;
	pid_t pid = pqueue->head->pid;
	struct pidnode *newhead = pqueue->head->next;
	free(pqueue->head);
	pqueue->head = newhead;
	if (newhead == NULL)
		pqueue->tail = newhead;
	pqueue->size--;
	return pid;
}

pid_t pqueue_isempty(struct pidqueue *pqueue)
{
	return pqueue->size == 0;
}

void free_pqueue(struct pidqueue *pqueue)
{
	struct pidnode *current = pqueue->head;
	while (current) {
		struct pidnode *discard_pid = current;
		current = current->next;
		free(discard_pid);
	}
	pqueue->head = NULL;
	pqueue->tail = NULL;
	pqueue->size = 0;
}
