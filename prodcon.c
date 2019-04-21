#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <memory.h>
#include <pthread.h>
#include <string.h>
#include "prodcon.h"

struct llist_node {
    struct llist_node *next;
    char *str;
};

static struct llist_node **heads; 
static assign_consumer_f assign_consumer;
static int producer_count;
static int consumer_count;
static int new_argc;
static char **new_argv;
static run_producer_f run_producer;
static run_consumer_f run_consumer;
_Thread_local int consumerID;
static char *sentinel = NULL;
pthread_mutex_t *lock;
pthread_cond_t *cond;

/**
 * pop a node off the start of the list.
 *
 * @param phead the head of the list. this will be modified by the call unless the list is empty (*phead == NULL).
 * @return NULL if list is empty or a pointer to the string at the top of the list. the caller is
 * incharge of calling free() on the pointer when finished with the string.
 */
char *pop(struct llist_node **phead)
{
    pthread_mutex_lock(&lock[consumerID]);
    while (*phead == NULL) {
        pthread_cond_wait(&cond[consumerID], &lock[consumerID]); 
    }
    char *s = (*phead)->str;
    struct llist_node *next = (*phead)->next;
    free(*phead);
    *phead = next; 
    pthread_mutex_unlock(&lock[consumerID]);
    return s;
}

/**
 * push a node onto the start of the list. a copy of the string will be made.
 * @param phead the head of the list. this will be modified by this call to point to the new node
 * being added for the string.
 * @param s the string to add. a copy of the string will be made and placed at the beginning of
 * the list.
 */
void push(struct llist_node **phead, const char *s)
{
    pthread_mutex_lock(&lock[consumerID]);

    struct llist_node *new_node = malloc(sizeof(*new_node));
    new_node->next = *phead;
    new_node->str = strdup(s);

    *phead = new_node;
    pthread_cond_signal(&cond[consumerID]);
    pthread_mutex_unlock(&lock[consumerID]);
}

/**
 * push a node onto the end of the list
 * @param phead the head of the list
 * being added for the string.
 * @param s the string to add.
 */
void pushSentinel(struct llist_node **phead, const char *s)
{
    pthread_mutex_lock(&lock[consumerID]);
    struct llist_node *new_node = malloc(sizeof(*new_node));
    new_node->str = sentinel;

    if (*phead == NULL) *phead = new_node;
    else {
        struct llist_node *curr = *phead;
        for(;curr != NULL; curr = curr->next) {
            if (curr->next == NULL) break;
        }
        curr->next = new_node;
    }   

    pthread_cond_signal(&cond[consumerID]);
    pthread_mutex_unlock(&lock[consumerID]);
}

void queue(int consumer, const char *str) 
{
    if (str == sentinel)
        pushSentinel(&heads[consumer], str);
    else
        push(&heads[consumer], str);
}

static void produce(const char *buffer)
{
    int hash = assign_consumer(consumer_count, buffer);
    queue(hash, buffer); 
}

static char *consume() {
    char *str = pop(&heads[consumerID]); 
    return str;
}

void do_usage(char *prog)
{
    printf("USAGE: %s shared_lib consumer_count producer_count ....\n", prog);
    exit(1);
}

void *start_producer_thread(void *i)
{
    run_producer((int)i, producer_count, produce, new_argc, new_argv);
    return 0;
}

void *start_consumer_thread(void *i)
{
    consumerID = (int) i;
    run_consumer((int) i, consume, new_argc, new_argv);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        do_usage(argv[0]);
    }

    char *shared_lib = argv[1];
    producer_count = (int) strtol(argv[2], NULL, 10);
    consumer_count = (int) strtol(argv[3], NULL, 10);

    new_argv = &argv[4];
    new_argc = argc - 4;
    
    setlinebuf(stdout);

    if (consumer_count <= 0 || producer_count <= 0) {
        do_usage(argv[0]);
    }

    void *dh = dlopen(shared_lib, RTLD_LAZY);

    run_producer = dlsym(dh, "run_producer");
    run_consumer = dlsym(dh, "run_consumer");
    assign_consumer = dlsym(dh, "assign_consumer");
    
    if (run_producer == NULL || run_consumer == NULL || assign_consumer == NULL) {
        printf("Error loading functions: prod %p cons %p assign %p\n", run_producer,
                run_consumer, assign_consumer);
        exit(2);
    }

    heads = calloc(consumer_count, sizeof(*heads));

    //Create a list of locks and conditions for each consumer list
    pthread_mutex_t lockTemp[consumer_count];
    pthread_cond_t condTemp[consumer_count];

    for (int i = 0; i < consumer_count; i++) {
        pthread_mutex_init(&lockTemp[i], NULL);
        pthread_cond_init(&condTemp[i], NULL);
    }
    
    lock = lockTemp;
    cond = condTemp;
    
    //Start Producer Threads
    pthread_t prod_threads[producer_count];
    for (int i = 0; i < producer_count; i++) {
        pthread_create(&prod_threads[i], NULL, start_producer_thread, (void *)(size_t) i);
    }

    //Start Consumers Thread
    pthread_t con_threads[consumer_count];
    for (int i = 0; i < consumer_count; i++) {
        pthread_create(&con_threads[i], NULL, start_consumer_thread, (void *)(size_t) i);
    }

    //Producer Finished
    for(int i = 0; i < producer_count; i++) {
        void *v;
        pthread_join(prod_threads[i], &v);
    }

    //Add Sentinel Nodes
    for(int i = 0; i < consumer_count; i++) {
        queue(i, sentinel);
    }

    //Signal consumers
    for(int i = 0; i < consumer_count; i++) {
        pthread_cond_signal(&cond[i]);
    }

    //Consumer Finished
    for(int i = 0; i < consumer_count; i++) {
        void *v;
        pthread_join(con_threads[i], &v);
    }

    return 0;
}
