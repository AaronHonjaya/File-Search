/*
 * Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

// Feature test macro for strtok_r (c.f., Linux Programming Interface p. 63)
#define _XOPEN_SOURCE 600

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "./CrawlFileTree.h"
#include "./DocTable.h"
#include "./MemIndex.h"
#include "libhw1/CSE333.h"

//////////////////////////////////////////////////////////////////////////////
// Helper function declarations, constants, etc
#define BUFF_SIZE 4
#define STARTING_QUERY_SIZE 1024
#define ERROR_CODE -1
#define EOF_CODE 0
#define SUCCESS_CODE 1

static void Usage(void);

//
static void ProcessQueries(DocTable* dt, MemIndex* mi);

/** Prompts user for a query and parses the input
 *  until the '\n' character is reached
 *
 * Arguments:
 * - f: the file to parse from (should be stdin);
 * - ret_str: where the parsed text will be returned to. The caller
 *            is responsible for freeing the parsed string. Unless an Error
 *            occurs.
 *
 * Returns:
 * - SUCCESS_CODE: on a successful parse
 * - EOF_CODE: when the end of the file has been reached.
 * - ERROR_CODE: when any errors occur.
 */
static int GetNextLine(FILE* f, char** ret_str);

/** Splits up a string by spaces into different query words
 *
 * Arguments:
 * - ret_str_array: is the return parameter. Should be an array that will store
 * the pointers to the different query words. The caller is responsible for
 *                  freeing this array by calling free() (e.g. free(queries));
 * - input: the string to parse from
 *
 * Returns:
 * - number of queries: on scucess
 * - ERROR_CODE: when any errors occur.
 */
static int GetQueries(char*** ret_str_array, char* input);

//////////////////////////////////////////////////////////////////////////////
// Main
int main(int argc, char** argv) {
  if (argc != 2) {
    Usage();
  }

  // Implement searchshell!  We're giving you very few hints
  // on how to do it, so you'll need to figure out an appropriate
  // decomposition into functions as well as implementing the
  // functions.  There are several major tasks you need to build:
  //
  //  - Crawl from a directory provided by argv[1] to produce and index
  //  - Prompt the user for a query and read the query from stdin, in a loop
  //  - Split a query into words (check out strtok_r)
  //  - Process a query against the index and print out the results
  //
  // When searchshell detects end-of-file on stdin (cntrl-D from the
  // keyboard), searchshell should free all dynamically allocated
  // memory and any other allocated resources and then exit.
  //
  // Note that you should make sure the fomatting of your
  // searchshell output exactly matches our solution binaries
  // to get full points on this part.
  fprintf(stdout, "Indexing '%s'\n", argv[1]);
  DocTable* dt;
  MemIndex* mi;
  if (!CrawlFileTree(argv[1], &dt, &mi)) {
    fprintf(stderr, "CrawlFileTree failed\n");
  }

  ProcessQueries(dt, mi);

  DocTable_Free(dt);
  MemIndex_Free(mi);
  return EXIT_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
// Helper function definitions

static void Usage(void) {
  fprintf(stderr, "Usage: ./searchshell <docroot>\n");
  fprintf(stderr,
          "where <docroot> is an absolute or relative "
          "path to a directory to build an index under.\n");
  exit(EXIT_FAILURE);
}

static void ProcessQueries(DocTable* dt, MemIndex* mi) {
  char** queries;
  char* input;
  int next_line_res;

  // repeat while the user input is valid.
  while ((next_line_res = GetNextLine(stdin, &input)) != ERROR_CODE &&
         next_line_res != EOF_CODE) {
    int num_queries = GetQueries(&queries, input);
    if (num_queries == ERROR_CODE) {
      free(input);
      break;
    }

    LinkedList* ll = MemIndex_Search(mi, queries, num_queries);
    if (ll == NULL) {
      // search has no matches so free and continue
      free(input);
      free(queries);
      continue;
    }

    // iterate through and print results.
    LLIterator* iter = LLIterator_Allocate(ll);
    SearchResult* sr;
    while (LLIterator_IsValid(iter)) {
      LLIterator_Get(iter, (LLPayload_t)&sr);
      char* name = DocTable_GetDocName(dt, sr->doc_id);
      fprintf(stdout, "  %s (%d)\n", name, sr->rank);
      LLIterator_Next(iter);
    }

    // free all allocated objects
    LLIterator_Free(iter);
    LinkedList_Free(ll, (LLPayloadFreeFnPtr)&free);
    free(input);
    free(queries);
  }
}

static int GetQueries(char*** ret_str_array, char* input) {
  char* curr_query;
  char* rest = input;
  int num_queries = 0;
  char** queries = (char**)malloc(sizeof(char*) * STARTING_QUERY_SIZE);
  if (queries == NULL) {
    fprintf(stderr, "Malloc Failed in GetQueries \n");
    return ERROR_CODE;
  }

  // keep parsing tokens and putting them in queries.
  while ((curr_query = strtok_r(rest, " \n", &rest)) != NULL) {
    queries[num_queries] = curr_query;
    num_queries++;
  }
  // set the return param.
  *ret_str_array = queries;
  return num_queries;
}

static int GetNextLine(FILE* f, char** ret_str) {
  fprintf(stdout, "enter query:\n");
  char buff[BUFF_SIZE];
  int content_size = BUFF_SIZE * 2;
  char* contents = (char*)malloc(content_size);
  if (contents == NULL) {
    fprintf(stderr, "Malloc Failed in GetNextLine \n");
    return ERROR_CODE;
  }
  // initialize for snprintf
  contents[0] = '\0';
  int buff_len = 0;
  do {
    if (fgets(buff, BUFF_SIZE, f) == NULL) {
      // End of file (CTRL+D) so terminate
      if (feof(f)) {
        printf("shutting down...\n");
        free(contents);
        return EOF_CODE;
      } else if (ferror(f) == EINTR || ferror(f) == EAGAIN) {
        // recoverable so continue
        continue;
      } else {
        // other error so terminate
        perror("fgets error in ParseUserInput\n");
        free(contents);
        return ERROR_CODE;
      }
    }
    buff_len = strlen(buff);
    // change everything to lower case
    for (int i = 0; i < buff_len; i++) {
      buff[i] = tolower(buff[i]);
    }

    int content_len = strlen(contents);
    // check if strin is too large
    if (buff_len + content_len >= content_size) {
      contents = (char*)realloc(contents, content_size * 2);
      content_size *= 2;
    }
    // copy over bytes from buff to contents
    snprintf(contents + content_len, BUFF_SIZE, "%s", buff);

    // end if string contains a '\n'
  } while (strchr(buff, '\n') == NULL);

  // set return param
  *ret_str = contents;
  return SUCCESS_CODE;
}
