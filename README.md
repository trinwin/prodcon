# prodcons
Create a multi-threaded producer/consumer engine. The engine will be supplied shared libraries with producer/consumer logic that the engine will run on.

## Compilation

The engine will be supplied shared libraries with producer/consumer logic that your engine will run.
The engine will be invoked as follows:

```
./prodcon shared_lib consumer_count producer_count optional_args ....
```

For example, wordcount.c is the "hello world" of parallel processing that counts all the words in a file. It outputs a list of words in the file with the number of times that word appeard. 

```
//Compile share library libwordcount.so
cc -fPIC -shared wordcount.c -o libwordcount.so -lpthread
```

```
//Run the program
gcc prodcon.c -o prodcon -ldl
./prodcon ./libwordcount.so 2 2 words
```
