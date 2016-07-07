#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void parent(int pipe, int child);

void child(int pipe, char** argv, char** environ);

int main(int argc, char** argv, char** environ) {
  int p[2];

  if (pipe(p)) {
    perror("[JAVA_WRAPPER] Error during the pipe creation.");
    exit(1);
  }

  int pid = fork();

  if (pid > 0) {
    close(p[1]);
    parent(p[0], pid);
  } else if (pid == 0) {
    close(p[0]);
    child(p[1], argv, environ);
  } else {
    perror("[JAVA_WRAPPER] Error during the fork creation.");
    exit(2);
  }
}

void parent(int pipe, int child) {

  int timeout = 60;

  char* timeoutEnv = getenv("WRAPPER_TIMEOUT");
  if (timeoutEnv) {
    timeout = atoi(timeoutEnv);
    if (!timeout) {
      perror("[JAVA_WRAPPER] Error parsing env WRAPPER_TIMEOUT");
      exit(5);
    }
  }

  printf("[JAVA_WRAPPER] Timeout: %d\n", timeout);
  fflush(stdout);

  const int bufferSize = 256;
  char buffer[bufferSize];
  fd_set rfds;
  struct timeval rtimeout;

  int hanging = 0;

  read:
  FD_ZERO(&rfds);
  FD_SET(pipe, &rfds);
  rtimeout.tv_sec = timeout;
  rtimeout.tv_usec = 0;
  while (select(pipe + 1, &rfds, NULL, NULL, &rtimeout) > 0) {
    fflush(stdout);
    // reset timeout
    rtimeout.tv_sec = timeout;
    rtimeout.tv_usec = 0;

    ssize_t size = read(pipe, buffer, bufferSize);

    if (size > 0) {
      write(1, buffer, size);
    } else if (size < 0) {
      perror("[JAVA_WRAPPER] Unable to read childs stdout.");
      exit(3);
    } else {
      break;
    }
  }

  int rc = waitpid(child, NULL, WNOHANG);
  if (rc > 0) {
    printf("[JAVA_WRAPPER] Process finished normally.\n");
    fflush(stdout);
  } else if (rc == 0) {
    if (hanging) {
      printf("[JAVA_WRAPPER] Process is hanging. Calling kill -9\n");
      fflush(stdout);
      kill(child, 9);
      waitpid(child, NULL, 0);
      exit(4);
    }
    if (rtimeout.tv_sec || rtimeout.tv_usec) {
      goto read;
    }
    printf("[JAVA_WRAPPER] Process is hanging. Calling kill -3\n");
    fflush(stdout);
    kill(child, 3);
    hanging = 1;
    timeout = 5;
    goto read;
  } else {
    perror("[JAVA_WRAPPER] Waitpid returned error code");
    exit(5);
  }
}

void child(int pipe, char** argv, char** environ) {
  char* javaCmd;

  char* javaHome = getenv("WRAPPER_JAVA_HOME");
  if (javaHome) {
    size_t len = strlen(javaHome) + 10;
    javaCmd = (char*) malloc(len * sizeof(char));
    strcpy(javaCmd, javaHome);
    strcat(javaCmd, "/bin/java");
  } else {
    javaCmd = (char*) malloc(14 * sizeof(char));
    strcpy(javaCmd, "/usr/bin/java");
  }

  printf("[JAVA_WRAPPER] Binary: %s\n", javaCmd);
  fflush(stdout);

  dup2(pipe, 1);
  dup2(pipe, 2);

  execve(javaCmd, argv, environ);
  free(javaCmd);
}
