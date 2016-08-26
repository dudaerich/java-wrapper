#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

pthread_mutex_t lock;
pthread_cond_t cond;
int hanging = 0;
int childExit = 0;

void parent(int pipe, int child);

void child(int pipe, char** argv, char** environ);

void* readChildOutput(void *arg);

void handleHanging(int hangingPid);

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
    exit(1);
  }
}

void parent(int pipe, int child) {
  pthread_t tid;
  int err;

  int args[] = {pipe, child};
  err = pthread_create(&tid, NULL, &readChildOutput, args);
  if (err) {
    perror("[JAVA_WRAPPER] cannot create thread");
    exit(1);
  }

  int handled = 0;

  pthread_mutex_lock(&lock);

  while (!childExit) {
    pthread_cond_wait(&cond, &lock);
    if (!handled && hanging) {
      handled = 1;
      printf("[JAVA_WRAPPER] Process is hanging\n");

      int pid = fork();
      if (pid > 0) {
        waitpid(pid, NULL, 0);
        printf("[JAVA_WRAPPER] calling kill -9\n");
        kill(child, 9);
      } else {
        handleHanging(child);
      }
    }
  }

  pthread_mutex_unlock(&lock);

  pthread_join(tid, NULL);

}

void child(int pipe, char** argv, char** environ) {
  const char javaBin[] = "/bin/java";
  const char defaultJava[] = "/usr/bin/java";
  char* javaCmd;

  char* javaHome = getenv("WRAPPER_JAVA_HOME");
  if (javaHome) {
    size_t len = strlen(javaHome) + sizeof(javaBin);
    javaCmd = (char*) malloc(len * sizeof(char));
    strcpy(javaCmd, javaHome);
    strcat(javaCmd, javaBin);
  } else {
    javaCmd = (char*) malloc(sizeof(defaultJava) * sizeof(char));
    strcpy(javaCmd, defaultJava);
  }

  printf("[JAVA_WRAPPER] Binary: %s\n", javaCmd);
  fflush(stdout);

  dup2(pipe, 1);
  dup2(pipe, 2);

  execve(javaCmd, argv, environ);
  free(javaCmd);
}

void* readChildOutput(void *arg) {
  int pipe = ((int*) arg)[0];
  int child = ((int*) arg)[1];
  int timeout = 60;

  char* timeoutEnv = getenv("WRAPPER_TIMEOUT");
  if (timeoutEnv) {
    timeout = atoi(timeoutEnv);
    if (!timeout) {
      perror("[JAVA_WRAPPER] Error parsing env WRAPPER_TIMEOUT");
      exit(2);
    }
  }

  printf("[JAVA_WRAPPER] Timeout: %d\n", timeout);
  fflush(stdout);

  const int bufferSize = 256;
  char buffer[bufferSize];
  fd_set rfds;
  struct timeval rtimeout;

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
      exit(1);
    } else {
      break;
    }
  }

  int rc = waitpid(child, NULL, WNOHANG);
  if (rc > 0) {
    printf("[JAVA_WRAPPER] Process finished.\n");
    fflush(stdout);
    pthread_mutex_lock(&lock);
    childExit = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
  } else if (rc == 0) {
    if (rtimeout.tv_sec || rtimeout.tv_usec) {
      goto read;
    }

    pthread_mutex_lock(&lock);
    if (!hanging) {
      hanging = 1;
      pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&lock);

    timeout = 10;
    goto read;
  } else {
    perror("[JAVA_WRAPPER] Waitpid returned error code");
    exit(1);
  }
}

void handleHanging(int hangingPid) {
  const char defaultScript[] = "kill -3 $0; sleep 5";

  char* exec = getenv("WRAPPER_HANG_EXEC");
  char* script = getenv("WRAPPER_HANG_SCRIPT");

  char hangingPidStr[50];
  sprintf(hangingPidStr, "%d", hangingPid);

  if (!script) {
    script = (char*) malloc(sizeof(defaultScript) * sizeof(char));
    strcpy(script, defaultScript);
  }

  if (exec) {
    printf("[JAVA_WRAPPER] Calling exec: %s\n", exec);
    fflush(stdout);
    if (execl(exec, exec, hangingPidStr, NULL)) {
      const char msg[] = "[JAVA_WRAPPER] error calling WRAPPER_HANG_EXEC=";
      char* error = (char*) malloc((sizeof(msg) + sizeof(exec)) * sizeof(char));
      strcpy(error, msg); strcpy(error, exec);
      perror(error);
      exit(2);
    }
  } else {
    printf("[JAVA_WRAPPER] Calling script: %s\n", script);
    fflush(stdout);
    if (execl("/bin/bash", "/bin/bash", "-c", script, hangingPidStr, NULL)) {
      const char msg[] = "[JAVA_WRAPPER] error calling WRAPPER_HANG_SCRIPT=";
      char* error = (char*) malloc((sizeof(msg) + sizeof(exec)) * sizeof(char));
      strcpy(error, msg); strcpy(error, exec);
      perror(error);
      exit(2);
    }
  }
}
