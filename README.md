# java-wrapper
Wrapper for java binary which monitors standard output and if the process does not produce any logs in defined timeout, it executes a user-defined script or executable file to investigate what happened with the Java process.

## Environment variables
* WRAPPER\_JAVA\_HOME - path pointing to installation directory of JDK. Java binary located in $WRAPPER\_JAVA\_HOME/bin/java is used. If it is not specified, the /usr/bin/java file is used.
* WRAPPER\_TIMEOUT - timeout in seconds. If the Java program does not produce any output in $WRAPPER\_TIMEOUT, the java-wrapper calls **hang handler** defined by environments and after that *kill -9* is called. Default value is 60.
* WRAPPER_HANG_SCRIPT - **hang handler** defined by bash script. It is called if the java process is hanging. The value is used as */bin/bash -c [value] JAVA_PID*. Default value is *kill -3 $0; sleep 5*. Give attention to *sleep 5*. The *kill -9* is called immediately after the **hang handler** finishes. *sleep 5* gives the Java process time to print the thread dump to standard output.
* WRAPPER_HANG_EXEC - **hang handler** defined by path to the executable. If the variable is defined, it has precedence before WRAPPER_HANG_SCRIPT. By default the variable is undefined.

## Build
```
gcc -lpthread -o java main.c
```
