/*
 * Soo Bin Kwon, Zach Abt
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include "threadsalive.h"

#define STACKSIZE 128000
#define MAX 50

/* ***************************** 
     stage 1 library functions
   ***************************** */
   
struct node{
	ucontext_t *context;
	struct node *next;
};

void list_append(ucontext_t *context, struct node **head) {
    struct node *new_node = (struct node*)malloc(sizeof(struct node));
    new_node->context = context;
	new_node->next = NULL;
	if(*head==NULL){
		*head = new_node;
		return;
	}	
    struct node *temp = *head;
    while(temp->next!=NULL){
    	temp = temp->next;
    }
    temp->next = new_node;
}

//delete the first item in the linked list
void list_delete(struct node **head){
    struct node *dead = *head;
    *head = dead->next;
    free(dead);
}

void list_clear(struct node *list) {
    while (list != NULL) {
        struct node *tmp = list;
        list = list->next;        
        ucontext_t* dead = tmp->context;
                
        free((*dead).uc_stack.ss_sp);
        free(dead);
        free(tmp);
    }
}

static ucontext_t main;
static struct node *head = NULL;
static struct node *current = NULL;

void ta_libinit(void) {
    return;
}

void ta_create(void (*func)(void *), void *arg) {
    ucontext_t* ctx = (ucontext_t*)malloc(sizeof(ucontext_t));
    unsigned char *stack = (unsigned char *)malloc(STACKSIZE);
    getcontext(ctx);
    (*ctx).uc_stack.ss_sp = stack;
    (*ctx).uc_stack.ss_size = STACKSIZE;
    (*ctx).uc_link = &main;  
    list_append(ctx,&head);
    makecontext(ctx, (void (*)(void))func, 1, arg);    
    return;
}

void ta_yield(void) {
    struct node *t = current;
    ucontext_t* ctx1 =  t->context;
    ucontext_t* ctx2;
    if(t->next==NULL){
        return;
    }else{
        ctx2 = (t->next)->context;    
    }
    list_append(t->context,&head);
    list_delete(&head);
    current = current->next; 
    swapcontext(ctx1,ctx2);  
    return;
}

int ta_waitall(void) {
    if(head==NULL){
        return -1;
    }
    current = head;
    swapcontext(&main, head->context);
    current = current->next;
    while(current!=NULL){       
        swapcontext(&main,(current)->context);
        current = current->next;
    }
    list_clear(head);
    return 0;
}

/* ***************************** 
     stage 2 library functions
   ***************************** */

void ta_sem_init(tasem_t *sem, int value) {
    sem->val = value;
    return;
}

void ta_sem_destroy(tasem_t *sem) {
    return;
}

void ta_sem_post(tasem_t *sem) {
    sem->val++;    
    if(sem->val <= 0){
        ta_yield();
    }
    return;  
}

void ta_sem_wait(tasem_t *sem) {   
    if(sem->val == 0){
        ta_yield();
    }
    else if(sem->val > 0){
        sem->val--;
    }
    return;
}

void ta_lock_init(talock_t *mutex) {
    tasem_t sem;
    ta_sem_init(&sem,1);
    mutex->bin_sem = sem;  
    return;
}

void ta_lock_destroy(talock_t *mutex) {
    ta_sem_destroy(&(mutex->bin_sem));
    return;
}

void ta_lock(talock_t *mutex) {
    ta_sem_wait(&(mutex->bin_sem));
}

void ta_unlock(talock_t *mutex) {
    ta_sem_post(&(mutex->bin_sem));
}


/* ***************************** 
     stage 3 library functions
   ***************************** */

void ta_cond_init(tacond_t *cond) {
    tasem_t sem;
    ta_sem_init(&sem,0);
    cond->con_sem = sem;  
    return;
}

void ta_cond_destroy(tacond_t *cond) {
    return;
}

void ta_wait(talock_t *mutex, tacond_t *cond) {
    cond->con_lock = *mutex;
    ta_unlock(mutex);
    ta_sem_wait(&(cond->con_sem));
}

void ta_signal(tacond_t *cond) {
    ta_sem_post(&(cond->con_sem));
    ta_lock(&(cond->con_lock));
}

