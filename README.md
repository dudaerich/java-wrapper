# java-wrapper
Wrapper for java binary which monitors standard output and if the process does not produce any logs, it sends kill -3 signal to print thread dump.

## Environment variables
* WRAPPER\_JAVA\_HOME - path pointing to installation directory of JDK. Java binary located in $WRAPPER\_JAVA\_HOME/bin/java is used. If it is not specified, the /usr/bin/java file is used.
* WRAPPER\_TIMEOUT - timeout in seconds. If the Java program does not produce any output for $WRAPPER\_TIMEOUT, the java-wrapper call _kill -3_ and after 5 seconds _kill -9_ on that process. Default value is 60.

## Build
```
gcc -o java main.c
```
