# proc-module
A kernel module that creates a readable/writable proc entry.

Example simple run:
```
$ make
$ insmod proc_module.ko
$ echo "Hello" > /proc/proc_module_file
$ echo "World" > /proc/proc_module_file
$ cat /proc/proc_module_file 
Hello
World
$ rmmod proc_module
$ make clean
```