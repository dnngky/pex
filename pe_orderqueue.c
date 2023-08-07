/*
 * comp2017 - assignment 3
 * dan nguyen
 * kngu7458
 */

#include "pe_exchange.h"

static void __free_oqueue(struct ordernode *onode)
{
    if (onode == NULL)
        return;
    __free_oqueue(onode->left);
    __free_oqueue(onode->right);
    free(onode);
}

static int get_ordertime(struct timeval start)
{
	struct timeval now;
	struct timeval diff;
	gettimeofday(&now, NULL);
	timersub(&now, &start, &diff);
	return (diff.tv_sec*1000000)+diff.tv_usec;	
}

static struct ordernode *init_node(struct order *order)
{
    struct ordernode *onode = malloc(sizeof(struct ordernode));
    onode->order = order;
    onode->parent = NULL;
    onode->left = NULL;
    onode->right = NULL;
    return onode;
}

static double key(struct orderqueue *oqueue, struct ordernode *u)
{
    double u_price = u->order->price;
    double u_time = u->order->time;
    if (oqueue->type == MAX_PRIORITYQUEUE)
        return u_price-(FLT_EPSILON*u_time);
    else
        return u_price+(FLT_EPSILON*u_time);
}

static void swap(struct ordernode *u, struct ordernode *v)
{
    struct order *u_order = u->order;
    struct order *v_order = v->order;
    u->order = v_order;
    v->order = u_order;
}

static struct ordernode *search(struct orderqueue *oqueue, struct ordernode *onode, struct order *order)
{
    if (onode == NULL)
        return NULL;
    
    // if onode's order info matches, return onode
    double target_key;
    if (oqueue->type == MAX_PRIORITYQUEUE)
        target_key = (double)order->price-(FLT_EPSILON*order->time);
    if (oqueue->type == MIN_PRIORITYQUEUE)
        target_key = (double)order->price+(FLT_EPSILON*order->time);
    if (key(oqueue, onode) == target_key)
        return onode;
    
    // else, recursively search the left and right subtrees of onode
    struct ordernode *target;
    if (oqueue->type == MAX_PRIORITYQUEUE)
    {
        if ((onode->left != NULL) &&
            (key(oqueue, onode->left) >= target_key) &&
            (target = search(oqueue, onode->left, order)) != NULL
        )
            return target;
        if ((onode->right != NULL) &&
            (key(oqueue, onode->right) >= target_key) &&
            (target = search(oqueue, onode->right, order)) != NULL
        )
            return target;
    }
    if (oqueue->type == MIN_PRIORITYQUEUE)
    {
        if ((onode->left != NULL) &&
            (key(oqueue, onode->left) <= target_key) &&
            (target = search(oqueue, onode->left, order)) != NULL
        )
            return target;
        if ((onode->right != NULL) &&
            (key(oqueue, onode->right) <= target_key) &&
            (target = search(oqueue, onode->right, order)) != NULL
        )
            return target;
    }
    // if nothing is found, the order does not exist
    return NULL;
}

static void upheap(struct orderqueue *oqueue, struct ordernode *onode)
{
    while (onode != oqueue->root)
    {
        if (oqueue->type == MAX_PRIORITYQUEUE && key(oqueue, onode) > key(oqueue, onode->parent))
            swap(onode, onode->parent);
        if (oqueue->type == MIN_PRIORITYQUEUE && key(oqueue, onode) < key(oqueue, onode->parent))
            swap(onode, onode->parent);
        onode = onode->parent;
    }
}

static void downheap(struct orderqueue *oqueue, struct ordernode *onode)
{
    while (onode->left != NULL || onode->right != NULL)
    {
        struct ordernode *child;
        if (onode->left == NULL)
            child = onode->right;
        else if (onode->right == NULL)
            child = onode->left;
        else if (oqueue->type == MAX_PRIORITYQUEUE && key(oqueue, onode->left) > key(oqueue, onode->right))
            child = onode->left;
        else if (oqueue->type == MIN_PRIORITYQUEUE && key(oqueue, onode->left) < key(oqueue, onode->right))
            child = onode->left;
        else
            child = onode->right;
        if (oqueue->type == MAX_PRIORITYQUEUE && key(oqueue, onode) < key(oqueue, child))
            swap(onode, child);
        if (oqueue->type == MIN_PRIORITYQUEUE && key(oqueue, onode) > key(oqueue, child))
            swap(onode, child);
        onode = child;
    }
}

static struct order *pop(struct orderqueue *oqueue, struct ordernode *onode)
{
    // if orderqueue is empty, return NULL
    if (oqueue->size == 0)
        return NULL;

    // if orderqueue only has one order (must be the root), simply remove it
    if (oqueue->size == 1) {
        if (onode != oqueue->root)
            return NULL;
        struct order *order = oqueue->root->order;
        free(oqueue->root);
        oqueue->root = NULL;
        oqueue->last = NULL;
        oqueue->size = 0;
        return order;
    }
    
    struct order *order = onode->order; // save original target order for return
    struct ordernode *cur_onode = oqueue->last;
    oqueue->size--;

    int early_return = 0;
    if (onode == oqueue->last)
        early_return = 1;
    else
        onode->order = oqueue->last->order; // replace target order with last order
    
    // search for previous last order
    int moved = 0;
    while (cur_onode->parent->right != cur_onode && cur_onode->parent != oqueue->root) {
        cur_onode = cur_onode->parent;
        moved = 1;
    }
    
    if (cur_onode->parent->right != cur_onode)
        cur_onode = (moved ? cur_onode->parent->right : cur_onode->parent);
    else
        cur_onode = cur_onode->parent->left;
    while (cur_onode->left != NULL && cur_onode->right != NULL)
        cur_onode = cur_onode->right;
    
    // update last order with previous last order (cur_onode)
    if (oqueue->last == oqueue->last->parent->left)
        oqueue->last->parent->left = NULL;
    else
        oqueue->last->parent->right = NULL;
    free(oqueue->last);
    oqueue->last = cur_onode;
    
    // if the target order is the last order, there is no need to restore heap property
    if (early_return)
        return order;
    
    // restore heap property
    if (onode == oqueue->root)
        downheap(oqueue, oqueue->root); // standard dequeue
    else if (oqueue->type == MAX_PRIORITYQUEUE && key(oqueue, onode) < key(oqueue, onode->parent))
        downheap(oqueue, onode);
    else if (oqueue->type == MIN_PRIORITYQUEUE && key(oqueue, onode) > key(oqueue, onode->parent))
        downheap(oqueue, onode);
    else
        upheap(oqueue, onode);
    
    return order;
}

struct orderqueue *init_oqueue(oqueue_t type)
{
    struct orderqueue *oqueue = malloc(sizeof(struct orderqueue));
    oqueue->type = type;
    gettimeofday(&(oqueue->inittime), NULL);
    oqueue->root = NULL;
    oqueue->last = NULL;
    oqueue->size = 0;
    return oqueue;
}

void oenqueue(struct orderqueue *oqueue, struct order *order)
{
    order->time = get_ordertime(oqueue->inittime);
    struct ordernode *new_onode = malloc(sizeof(struct ordernode));
    new_onode->order = order;
    new_onode->parent = NULL;
    new_onode->left = NULL;
    new_onode->right = NULL;
    struct ordernode *onode = oqueue->last;
    oqueue->size++;

    // if orderqueue is empty, simply set new order as the root
    if (onode == NULL) {
        oqueue->root = new_onode;
        oqueue->last = new_onode;
        return;
    }
    // if orderqueue only has the root, simply add new order as its left child
    if (onode == oqueue->root) {
        new_onode->parent = oqueue->root;
        oqueue->root->left = new_onode;
    }
    // else, add new order to next available space on the orderqueue
    else
    {
        int moved = 0;
        while (onode->parent->left != onode && onode->parent != oqueue->root) {
            onode = onode->parent;
            moved = 1;
        }
        if (moved && onode->parent->left == onode)
            onode = onode->parent->right;
        else
            onode = onode->parent;
        while (onode->left != NULL && onode->right != NULL)
            onode = onode->left;
        new_onode->parent = onode;
        if (onode->left == NULL)
            onode->left = new_onode;
        else
            onode->right = new_onode;
    }
    // update last order with new order
    oqueue->last = new_onode;

    // restore heap property
    upheap(oqueue, new_onode);
}

struct order *odequeue(struct orderqueue *oqueue)
{
    return pop(oqueue, oqueue->root);
}

struct order *opopqueue(struct orderqueue *oqueue, struct order *order)
{
    struct ordernode *target = search(oqueue, oqueue->root, order);
    if (target == NULL)
        return NULL;
    return pop(oqueue, target);
}

struct order *opeek(struct orderqueue *oqueue)
{
    return oqueue->root->order;
}

int oqueue_isempty(struct orderqueue *oqueue)
{
    return oqueue->size == 0;
}

void free_oqueue(struct orderqueue *oqueue)
{
    __free_oqueue(oqueue->root);
    oqueue->root = NULL;
    oqueue->last = NULL;
    oqueue->size = 0;
}
