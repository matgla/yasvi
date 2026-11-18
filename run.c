/*
 Copyright (c) 2026 Mateusz Stadnik

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include "run.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "make.h"

extern char** environ;

// Everything execve reads: static, for the reason in run.h. The script is a
// few hundred bytes; the rest holds a copy of the environment (PATH and HOME
// matter to the build), and variables that do not fit are left out.
#define RUN_ARENA_BYTES 3072
#define RUN_MAX_ENV 48
#define RUN_DEFAULT_PATH "PATH=/bin:/usr/bin"

static char g_run_arena[RUN_ARENA_BYTES];
static size_t g_run_arena_used;
static char* g_run_argv[4];
static char* g_run_envp[RUN_MAX_ENV + 1];
static char* g_run_out_path;
static bool g_run_stdin_null;  // the child reads /dev/null, not the terminal
static char* g_run_program;

static char* run_arena_dup(const char* text) {
  const size_t length = strlen(text) + 1;
  if (g_run_arena_used + length > sizeof(g_run_arena)) {
    return NULL;
  }
  char* out = g_run_arena + g_run_arena_used;
  memcpy(out, text, length);
  g_run_arena_used += length;
  return out;
}

// TERM for the next environment copy, replacing the inherited one (NULL:
// keep it).
static const char* g_run_term;

// Copies the environment after whatever the arena already holds.
static void run_copy_environment(void) {
  int count = 0;
  bool has_path = false;
  if (g_run_term != NULL) {
    char* term = run_arena_dup(g_run_term);
    if (term != NULL) {
      g_run_envp[count++] = term;
    }
  }
  for (char** entry = environ; entry != NULL && *entry != NULL; ++entry) {
    if (count == RUN_MAX_ENV - 1) {
      break;  // one slot stays free for a default PATH
    }
    if (g_run_term != NULL && strncmp(*entry, "TERM=", 5) == 0) {
      continue;
    }
    char* copy = run_arena_dup(*entry);
    if (copy == NULL) {
      continue;
    }
    if (strncmp(copy, "PATH=", 5) == 0) {
      has_path = true;
    }
    g_run_envp[count++] = copy;
  }
  if (!has_path) {
    char* path = run_arena_dup(RUN_DEFAULT_PATH);
    if (path != NULL) {
      g_run_envp[count++] = path;
    }
  }
  g_run_envp[count] = NULL;
}

// argv = { "bash", "-c", script }, envp = a copy of environ.
static bool run_prepare_shell(const char* script) {
  g_run_arena_used = 0;
  g_run_out_path = NULL;
  g_run_argv[0] = run_arena_dup("bash");
  g_run_argv[1] = run_arena_dup("-c");
  g_run_argv[2] = run_arena_dup(script);
  g_run_argv[3] = NULL;
  g_run_program = run_arena_dup(MAKE_SHELL);
  if (g_run_argv[0] == NULL || g_run_argv[1] == NULL || g_run_argv[2] == NULL ||
      g_run_program == NULL) {
    return false;  // a script longer than the arena
  }
  run_copy_environment();
  return true;
}

static int run_wait_for(pid_t pid) {
  int status = 0;
  pid_t done;
  do {
    done = waitpid(pid, &status, 0);
  } while (done < 0 && errno == EINTR);
  if (done != pid) {
    return -1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 128 + WTERMSIG(status);
}

// vfork + execve of g_run_argv/g_run_envp, then waitpid.
static int run_spawn_and_wait(const char* program) {
  fflush(stdout);
  fflush(stderr);
  pid_t pid = vfork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    // The child shares vi's memory until execve: only syscalls here.
    if (g_run_stdin_null) {
      int fd = open("/dev/null", O_RDONLY);
      if (fd >= 0) {
        dup2(fd, 0);
        close(fd);
      }
    }
    if (g_run_out_path != NULL) {
      int fd = open(g_run_out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        _exit(127);
      }
      dup2(fd, 1);
      dup2(fd, 2);
      close(fd);
    }
    execve(program, g_run_argv, g_run_envp);
    _exit(127);
  }
  return run_wait_for(pid);
}

int run_shell_wait(const char* script) {
  if (!run_prepare_shell(script)) {
    return -1;
  }
  return run_spawn_and_wait(g_run_program);
}

void run_shell_exec(const char* script) {
  if (!run_prepare_shell(script)) {
    return;
  }
  fflush(stdout);
  fflush(stderr);
  execve(g_run_program, g_run_argv, g_run_envp);
}

int run_to_file(const char* program, const char* arg, const char* out_path) {
  g_run_arena_used = 0;
  const char* name = strrchr(program, '/');
  // toybox picks the applet by argv[0]'s name
  g_run_argv[0] = run_arena_dup(name != NULL ? name + 1 : program);
  g_run_argv[1] = run_arena_dup(arg);
  g_run_argv[2] = NULL;
  g_run_out_path = run_arena_dup(out_path);
  char* program_copy = run_arena_dup(program);
  if (g_run_argv[0] == NULL || g_run_argv[1] == NULL || g_run_out_path == NULL ||
      program_copy == NULL) {
    g_run_out_path = NULL;
    return -1;
  }
  run_copy_environment();
  const int status = run_spawn_and_wait(program_copy);
  g_run_out_path = NULL;
  return status;
}

int run_shell_to_file(const char* script, const char* out_path) {
  if (!run_prepare_shell(script)) {
    return -1;
  }
  g_run_out_path = run_arena_dup(out_path);
  if (g_run_out_path == NULL) {
    return -1;
  }
  g_run_stdin_null = true;
  const int status = run_spawn_and_wait(g_run_program);
  g_run_stdin_null = false;
  g_run_out_path = NULL;
  return status;
}

int run_shell_piped(int* to_shell, int* from_shell) {
  int input[2];
  int output[2];
  if (pipe(input) != 0) {
    return -1;
  }
  if (pipe(output) != 0) {
    close(input[0]);
    close(input[1]);
    return -1;
  }
  // vi's ends: never inherited by the shell or anything else vi starts.
  fcntl(input[1], F_SETFD, FD_CLOEXEC);
  fcntl(output[0], F_SETFD, FD_CLOEXEC);
  g_run_arena_used = 0;
  g_run_out_path = NULL;
  g_run_argv[0] = run_arena_dup("bash");
  g_run_argv[1] = NULL;
  g_run_program = run_arena_dup(MAKE_SHELL);
  g_run_term = "TERM=dumb";
  run_copy_environment();
  g_run_term = NULL;
  pid_t pid = -1;
  if (g_run_argv[0] != NULL && g_run_program != NULL) {
    fflush(stdout);
    fflush(stderr);
    pid = vfork();
    if (pid == 0) {
      // The child shares vi's memory until execve: only syscalls here.
      dup2(input[0], 0);
      dup2(output[1], 1);
      dup2(output[1], 2);
      close(input[0]);
      close(input[1]);
      close(output[0]);
      close(output[1]);
      execve(g_run_program, g_run_argv, g_run_envp);
      _exit(127);
    }
  }
  close(input[0]);
  close(output[1]);
  if (pid < 0) {
    close(input[1]);
    close(output[0]);
    return -1;
  }
  fcntl(input[1], F_SETFL, O_NONBLOCK);
  fcntl(output[0], F_SETFL, O_NONBLOCK);
  *to_shell = input[1];
  *from_shell = output[0];
  return pid;
}

char* run_expand_command(const char* command, const char* filename, bool* no_name) {
  if (no_name != NULL) {
    *no_name = false;
  }
  char* quoted = NULL;
  size_t length = 0;
  for (const char* p = command; *p != '\0'; ++p) {
    if (p[0] == '\\' && p[1] == '%') {
      ++p;
      ++length;
    } else if (*p == '%') {
      if (filename == NULL) {
        free(quoted);
        if (no_name != NULL) {
          *no_name = true;
        }
        return NULL;
      }
      if (quoted == NULL && (quoted = make_shell_quote(filename)) == NULL) {
        return NULL;
      }
      length += strlen(quoted);
    } else {
      ++length;
    }
  }
  char* out = malloc(length + 1);
  if (out == NULL) {
    free(quoted);
    return NULL;
  }
  char* q = out;
  for (const char* p = command; *p != '\0'; ++p) {
    if (p[0] == '\\' && p[1] == '%') {
      *q++ = *++p;
    } else if (*p == '%') {
      const size_t n = strlen(quoted);
      memcpy(q, quoted, n);
      q += n;
    } else {
      *q++ = *p;
    }
  }
  *q = '\0';
  free(quoted);
  return out;
}

void run_wait_for_enter(void) {
  char c = 0;
  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == '\n' || c == '\r') {
      return;
    }
  }
}
