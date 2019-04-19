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

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

//LinkedList
//lock for each for each thread

static struct llist_node **heads; //Array of head
static assign_consumer_f assign_consumer; //assign a string to a customer - returrn customer number that the string is assigned to
_Thread_local int my_consumer_number;
static int producer_count;
static int consumer_count;
static int new_argc;
static char **new_argv;
static run_producer_f run_producer;
static run_consumer_f run_consumer;
static char *sentinel = "sentinel";


/**
 * pop a node off the start of the list.
 *
 * @param phead the head of the list. this will be modified by the call unless the list is empty (*phead == NULL).
 * @return NULL if list is empty or a pointer to the string at the top of the list. the caller is
 * incharge of calling free() on the pointer when finished with the string.
 */
char *pop(struct llist_node **phead)
{
    pthread_mutex_lock(&lock);
    // printf("%s\n", (*phead)->str);
    while (*phead == NULL) {
        pthread_cond_wait(&cond, &lock); 
        // pthread_mutex_unlock(&lock);
        // return NULL;
    }

    char *s = (*phead)->str;
    //printf("Consumer - Pop: %s\n", s);
    struct llist_node *next = (*phead)->next;
    free(*phead);
    *phead = next; //new head
//    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
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
    pthread_mutex_lock(&lock);
    struct llist_node *new_node = malloc(sizeof(*new_node));
    new_node->next = *phead;
	printf("Producer  - Push: %s\n", s);
    if (s == sentinel)
        new_node->str = sentinel;
    else 
        new_node->str = strdup(s);

    *phead = new_node;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

// void pushEnd(struct llist_node **phead, const char *s)
// {
//     pthread_mutex_lock(&lock);
//     struct llist_node *new_node = malloc(sizeof(*new_node));
//     new_node->str = sentinel;

//     struct llist_node *currNode = malloc(sizeof(*currNode));

//     while (currNode->next != NULL){
//         printf("Here %s\n", currNode->str);
//         currNode = currNode->next;

//     }
//     currNode->next = new_node;
//     printf("Producer  - Push: %s\n", s);
//     pthread_cond_signal(&cond);
//     pthread_mutex_unlock(&lock);
// }

// void queueSentinel(int consumer, const char *str) // push
// {
//     pushEnd(&heads[consumer], str);
//     //printf("Producer assign to Con %d --> %s\n", hash, buffer);
// }

// the array of list heads. the size should be equal to the number of consumers

void queue(int consumer, const char *str) // push
{
    push(&heads[consumer], str);
    //printf("Producer assign to Con %d --> %s\n", hash, buffer);
}

static void produce(const char *buffer)
{
    int hash = assign_consumer(consumer_count, buffer); //number of consumers
    printf("Producer assign to Con %d --> %s\n", hash, buffer);
    queue(hash, buffer); // push customer + its string to linked list
}

static char *consume() {
    char *str = pop(&heads[my_consumer_number]); // // push customer + its string from linked list
    printf("Consumer %d Pop --> %s\n", my_consumer_number, str);
    if (str == sentinel) {
        //printf("Reach %s of Consumer %d\n", str, my_consumer_number);
        return NULL;
    }
    else return str;
    //return null when consume 
}

void do_usage(char *prog)
{
    printf("USAGE: %s shared_lib consumer_count producer_count ....\n", prog);
    exit(1);
}

void *start_producer_thread(void *i)
{
    run_producer((u_int64_t)i, producer_count, produce, new_argc, new_argv);
    return 0;
}

void *start_consumer_thread(void *i)
{
    my_consumer_number = (u_int64_t) i;
    run_consumer((u_int64_t) i, consume, new_argc, new_argv);
    return 0;
}

void printAll(){
    for(int i = 0; i < consumer_count; i++){
        // printf("Consumer %d has %s\n", i, heads[my_consumer_number]->str);
    }
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

    // load the producer, consumer, and assignment functions from the library
    run_producer = dlsym(dh, "run_producer");
    run_consumer = dlsym(dh, "run_consumer");
    assign_consumer = dlsym(dh, "assign_consumer");
    
    if (run_producer == NULL || run_consumer == NULL || assign_consumer == NULL) {
        printf("Error loading functions: prod %p cons %p assign %p\n", run_producer,
                run_consumer, assign_consumer);
        exit(2);
    }

    //Create customer
    heads = calloc(consumer_count, sizeof(*heads));
    
    //Start Producers
    pthread_t prod_threads[producer_count];
    for (int i = 0; i < producer_count; i++) {
        pthread_create(&prod_threads[i], NULL, start_producer_thread, (void *)(size_t) i);
    }


    for(int i = 0; i < consumer_count; i++) {
        // printf("Add %s to %d\n", sentinel, i);
        queue(i, sentinel);
        printf("Add %s to Con %d\n", sentinel, i);
    }

    //Start Consumers
    pthread_t con_threads[consumer_count];
    for (int i = 0; i < consumer_count; i++) {
        pthread_create(&con_threads[i], NULL, start_consumer_thread, (void *)(size_t) i);
    }

    //Producer Finished
    for(int i = 0; i < producer_count; i++) {
        void *v;
        pthread_join(prod_threads[i], &v);
        printf("Producer DONE\n");
        //all producers are done 
        //sentinel - signal consumers that producers are done 
    }

    
    //printAll();

    //Add Sentinel Nodes
    // for(int i = 0; i < consumer_count; i++) {
    //     // printf("Add %s to %d\n", sentinel, i);
    //     queue(i, sentinel);
    //     printf("Add %s to Con %d\n", sentinel, i);
    // }

    //Consumer Finished
    for(int i = 0; i < consumer_count; i++) {
        void *v;
        pthread_join(con_threads[i], &v);
        printf("Consumer DONE\n");
    }
    printf("End\n");
    return 0;

    // 2 producers 15 consumers  - 
    // sentinel - 
    // How do you know when all producers are dead
    //
    // How do I check if they are running properly 
    /**
    sentinel - all producers are done
    */
}
