#include "prodcon.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <memory.h>
#include <pthread.h>

/*
 * this function will take a file split it into equal sized chunks. each consumer will process
 * their corresponding chunks. to handle word overlap, each consumer except the first will skip
 * the first word of their chunk. quick and dirty... might have bugs...
 */
void run_producer(int num, int producer_count, produce_f produce, int argc, char **argv)
{
    if (argc != 1) {
        if (num == 0) {
            printf("wordcount requires exactly one file to be passed on commandline got %d:\n",
                    argc);
            for (int i = 0; i < argc; i++) {
                printf("%d: %s\n", i, argv[i]);
            }
            exit(2);
        }
        return;
    }
    FILE *fh = fopen(argv[0], "r");
    if (fh == NULL) {
        perror(argv[0]);
        exit(3);
    }
    fseek(fh, 0, SEEK_END);
    long file_size = ftell(fh);
    long start = file_size * num / producer_count;
    long end = file_size * (num+1) / producer_count;
    if (num == producer_count-1) {
        end = file_size;
    }
    fseek(fh, start, SEEK_SET);

    // peek into the stream to see if we are at a space
    int start_with_space = isspace(fgetc(fh));
    fseek(fh, start, SEEK_SET);

    if (num != 0 && !start_with_space) {
        // if we don't start with a space then we skip the first word since the
        // producer before us will read it.
        char *ptr;
        fscanf(fh, "%ms", &ptr);
        free(ptr);
    }
    while (ftell(fh) <= end) {
        char *s;
        int rc = fscanf(fh, "%ms", &s);
        if (rc != 1) {
            break;
        }
        produce(s);
        free(s);
        // skip over the whitespace, we have to go back one after we find a non
        // whitespace character
        while (isspace(fgetc(fh)));
        fseek(fh, -1, SEEK_CUR);
    }
}

// simple hashmap
static const int hash_table_size = 1000;

struct hash_entry_s {
    char *str;
    int count;
    // link list for collisions
    struct hash_entry_s *next;
};


int hash_string(int count, const char *str)
{
    int total = 0;
    // silly hash function. not the best, but easy to write.
    for (int i = 0; str[i]; i++) {
        total += total << 5;
        total ^= str[i];
        total ^= total >> 8;
    }
    return abs(total) % count;
}

struct hash_entry_s *find_entry(struct hash_entry_s **hash_array, int hash, const char *str)
{
    struct hash_entry_s *entry = hash_array[hash];
    while (entry) {
        if (strcmp(str, entry->str) == 0) {
            break;
        }
        entry = entry->next;
    }
    return entry;
}

void add_string(struct hash_entry_s **hash_array, int count, const char *str)
{
    int hash = hash_string(count, str);
    struct hash_entry_s *entry = find_entry(hash_array, hash, str);
    if (entry) {
        entry->count++;
    } else {
        entry = malloc(sizeof(*entry));
        entry->str = strdup(str);
        entry->count = 1;
        entry->next = hash_array[hash];
        hash_array[hash] = entry;
    }
}

void dump_table(int num, struct hash_entry_s **hash_array, int count) {
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    fflush(stdout);
    pthread_mutex_lock(&mutex);
    fflush(stdout);
    for (int i = 0; i < count; i++) {
        struct hash_entry_s *entry = hash_array[i];
        while (entry) {
            printf("%s %d\n", entry->str, entry->count);
	    free(entry->str);
	    struct hash_entry_s *old = entry;
            entry = entry->next;
	    free(old);
        }
    }
    pthread_mutex_unlock(&mutex);
    fflush(stdout);
}

/*
 * this consumer will put everything in a hashtable that included a count. at the end
 * it will dump the keys and counts.
 */
void run_consumer(int num, consume_f consume, int argc, char **argv)
{
    struct hash_entry_s *hash_array[hash_table_size];
    memset(hash_array, 0, sizeof(hash_array));
    char *str;
    while ((str = consume())) {
        add_string(hash_array, hash_table_size, str);
        free(str);
    }
    dump_table(num, hash_array, hash_table_size);
}


int assign_consumer(int count, char *str)
{
    return hash_string(count, str);
}
