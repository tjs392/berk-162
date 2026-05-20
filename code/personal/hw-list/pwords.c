#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "word_count.h"
#include "word_helpers.h"

typedef struct thread_arg {
  char* filename;
  word_count_list_t* wclist;
} thread_arg_t;


void* count_words_thread(void* raw_arg) {
  thread_arg_t* arg = (thread_arg_t*) raw_arg;

  FILE* f = fopen(arg->filename, "r");
  if (f == NULL) {
    perror("fopen");
    pthread_exit(NULL);
  }
  count_words(arg->wclist, f);
  fclose(f);

  pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
  word_count_list_t wclist;
  init_words(&wclist);

  int nthreads = argc - 1;
  if (nthreads == 0) {
    count_words(&wclist, stdin);
  } else {
    pthread_t* threads = malloc(nthreads * sizeof(pthread_t));
    thread_arg_t* args = malloc(nthreads * sizeof(thread_arg_t));

    for (int i = 0; i < nthreads; i++) {
      args[i].filename = argv[i + 1];
      args[i].wclist = &wclist;
      pthread_create(&threads[i], NULL, count_words_thread, &args[i]);
    }

    for (int i = 0; i < nthreads; i++) {
      pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);
  }

  wordcount_sort(&wclist, less_count);
  fprint_words(&wclist, stdout);

  return 0;
}