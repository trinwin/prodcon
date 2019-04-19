#ifndef PRODUCER_CONSUMER_PRODCON_H
#define PRODUCER_CONSUMER_PRODCON_H

// these next two functions pointers are defined to allow the producer consumer engine to pass
// functions to collect strings produced by a plugin and to provide strings to be consumed by a
// consumer
/**
 * will be called by the producer to hand off a string to the producer consumer engine.
 */
typedef void (*produce_f)(const char *buffer);

/**
 * will be called by a consumer to get the next string to be consumed. this call may block if
 * producers are still running. a NULL will be returned once all producers have finished and
 * there are no more strings for the consumer.
 */
typedef char *(*consume_f)(void);

// the following functions will be provided by plugin libraries: a library must export a function
// named "run_producer", "run_consumer", and "assign_consumer".
/**
 * this function will be called to start a producer running. once the producer has nothing more
 * to produce, it should return from the function.
 * @param num the producer number starting at 0.
 * @param producer_count the number of producers that will be started.
 * @param produce the function to call when a string has been produced.
 * @param argc the command line argument count passed to the plugin
 * @param argv the command line arguments passed to the plugin
 *
 */
typedef void (*run_producer_f)(int num, int producer_count, produce_f produce, int argc, char
**argv);

/**
 * this function will be called to start a consumer running. once the consumer has nothing more
 * to consume, it should return from the function.
 * @param num the consumer number starting at 0.
 * @param consume the function to call to get the next string to be consumed.
 * @param argc the command line argument count passed to the plugin
 * @param argv the command line arguments passed to the plugin
 *
 */
typedef void (*run_consumer_f)(int num, consume_f consume, int argc, char **argv);

/**
 * this function assigns a string to a consumer. function must return an integer from zero up to
 * but not including consumer_count. the returned integer indicates the number the string is
 * assigned to.
 */
typedef int (*assign_consumer_f)(int consumer_count, const char *buffer);

#endif //PRODUCER_CONSUMER_PRODCON_H
