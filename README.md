# prodcons
Create a multi-threaded producer/consumer engine. The engine will be supplied shared libraries with producer/consumer logic that the engine will run on.

## Compilation

```bash
cc -fPIC -shared wordcount.c -o libwordcount.so -lpthread
```
```bash
gcc prodcon.c -o prodcon -ldl
```
```bash
./prodcon shared_lib consumer_count producer_count optional_args ....
```
```bash
./prodcon ./libwordcount.so 2 2 words
```
