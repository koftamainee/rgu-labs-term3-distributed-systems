#include <stdio.h>
//
#include <ctype.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MAX_INPUT_LENGTH 256

#define MEMORY_ALLOCATION_ERROR 1
#define DEREFERENCING_NULL_ERROR 2
#define PARSING_ERROR 3
#define FILE_PARSING_ERROR 4
#define INCORRECT_PASSWORD_ERROR 5
#define FILE_OPENING_ERROR 6
#define NO_SUCH_USERNAME_ERROR 7
#define SELF_BAN_ERROR 8

#define ADMIN_PASSWORD 52

#define USERS_FILENAME ".users"

typedef struct {
  unsigned int uid;
  char login[7];
  unsigned int pin;
} user_entry;

struct termios saved_term;

int trim_string(char *s);

int get_users_from_file(user_entry **users, size_t *users_count);
int login_user(user_entry const *user);
int register_user(char const *username, unsigned int *pin_placeholder);

int shell_main_loop(user_entry const *users, int *users_bans,
                    size_t users_count, unsigned int current_uid);
int handle_time();
int handle_date();
int handle_howmuch(char *user_input);
int handle_sanctions(char *user_input, user_entry const *users, int *users_bans,
                     size_t users_count, unsigned int current_uid);

void print_error(int errno);

void ignore_sigint(int sig);
void disable_echo();
void restore_terminal();

char *command_generator(const char *text, int state);

char **completion(const char *text, int start, int end);

int main(void) {
  int err = 0;
  user_entry *users = NULL;
  void *for_realloc = NULL;
  size_t users_count = 0;
  size_t i = 0;
  int found = 0;
  size_t len = 0;
  unsigned int pin = 0;
  int retry = 0;
  FILE *fapp = NULL;
  int *users_bans = NULL;  // better to use bitfield idk
  unsigned int current_uid = 0;

  char *user_input = NULL;

  err = get_users_from_file(&users, &users_count);
  if (err != 0) {
    print_error(err);
    return err;
  }
  users_bans = (int *)malloc(users_count * sizeof(int));
  if (users_bans == NULL) {
    free(users);
    print_error(MEMORY_ALLOCATION_ERROR);
    return MEMORY_ALLOCATION_ERROR;
  }

  memset(users_bans, 0, users_count * sizeof(int));

  while (1) {
    rl_attempted_completion_function = NULL;
    current_uid = 0;
    found = 0;
    retry = 0;
    user_input = readline("shell login: ");
    if (user_input == NULL) {
      break;
    }

    len = trim_string(user_input);

    if (len == 0) {
      free(user_input);
      continue;
    }

    if (len > 6) {
      printf("Login can not be longer than 6 symbols. Try again.\n");
      free(user_input);
      continue;
    }

    for (i = 0; i < users_count; i++) {
      if (strcmp(users[i].login, user_input) == 0) {
        if (users_bans[users[i].uid - 1] == 1) {
          printf("You got banned for current session. How sad!\n");
          retry = 1;
          break;
        }
        err = login_user(users + i);
        if (err == INCORRECT_PASSWORD_ERROR) {
          printf("Incorrect PIN, try again.\n");
          retry = 1;
          break;
        } else if (err != 0) {
          print_error(err);
          free(user_input);
          free(users);
          free(users_bans);
          return err;
        }
        found = 1;
        current_uid = i + 1;
      }
    }

    if (retry) {
      free(user_input);
      continue;
    }

    if (!found) {
      err = register_user(user_input, &pin);
      if (err == INCORRECT_PASSWORD_ERROR) {
        printf("Incorrect PIN format, try again.\n");
        free(user_input);
        continue;
      }
      if (err != 0) {
        print_error(err);
        free(users);
        free(user_input);
        free(users_bans);
        return err;
      }
      for_realloc =
          (user_entry *)realloc(users, (users_count + 1) * sizeof(user_entry));
      if (for_realloc == NULL) {
        print_error(MEMORY_ALLOCATION_ERROR);
        free(users);
        free(user_input);
        free(users_bans);
        return MEMORY_ALLOCATION_ERROR;
      }
      users = for_realloc;
      for_realloc = NULL;
      strcpy(users[users_count].login, user_input);
      users[users_count].pin = pin;
      users[users_count].uid =
          (users_count == 0) ? 1 : users[users_count - 1].uid + 1;

      for_realloc = realloc(users_bans, (users_count + 1) * sizeof(int));
      if (for_realloc == NULL) {
        print_error(MEMORY_ALLOCATION_ERROR);
        free(users);
        free(user_input);
        free(users_bans);
        return MEMORY_ALLOCATION_ERROR;
      }
      users_bans = for_realloc;
      users_bans[users_count] = 0;
      for_realloc = NULL;

      fapp = fopen(USERS_FILENAME, "ab");
      if (fapp == NULL) {
        print_error(FILE_OPENING_ERROR);
        free(user_input);
        free(users);
        free(users_bans);
        return FILE_OPENING_ERROR;
      }
      fwrite(users + users_count, sizeof(user_entry), 1, fapp);
      fclose(fapp);

      users_count++;
      free(user_input);
      continue;
    }

    free(user_input);
    err = shell_main_loop(users, users_bans, users_count, current_uid);
    signal(SIGINT, SIG_DFL);
    if (err != 0) {
      print_error(err);
      free(users);
      free(user_input);
      free(users_bans);
      return err;
    }
  }
  free(users);
  free(users_bans);
}

int shell_main_loop(user_entry const *users, int *users_bans,
                    size_t users_count, unsigned int current_uid) {
  if (users == NULL || users_bans == NULL) {
    return DEREFERENCING_NULL_ERROR;
  }
  char *user_input = NULL;
  int err = 0;
  size_t len = 0;

  signal(SIGINT, ignore_sigint);
  rl_attempted_completion_function = completion;

  while (1) {
    user_input = readline("shell -> ");
    if (user_input == NULL) {
      return 0;
    }

    len = strlen(user_input);
    while (len > 0 && isspace((unsigned char)user_input[len - 1])) {
      user_input[len - 1] = '\0';
      --len;
    }

    if (len == 0) {
      free(user_input);
      continue;
    }

    add_history(user_input);

    if (strcmp(user_input, "Logout") == 0) {
      clear_history();
      free(user_input);
      break;
    }

    if (strcmp(user_input, "Time") == 0) {
      err = handle_time();
      if (err != 0) {
        free(user_input);
        return err;
      }
    } else if (strcmp(user_input, "Date") == 0) {
      err = handle_date();
      if (err != 0) {
        free(user_input);
        return err;
      }
    } else if (strncmp(user_input, "Howmuch ", 8) == 0) {
      err = handle_howmuch(user_input);
      if (err == PARSING_ERROR) {
        printf("Failed to parse input data. Try again\n");
        free(user_input);
        continue;
      }
      if (err != 0) {
        free(user_input);
        return err;
      }
    } else if (strncmp(user_input, "Sanctions ", 10) == 0) {
      err = handle_sanctions(user_input, users, users_bans, users_count,
                             current_uid);
      if (err == PARSING_ERROR) {
        printf("Failed to parse input data. Try again\n");
        free(user_input);
        continue;
      } else if (err == INCORRECT_PASSWORD_ERROR) {
        printf("Incorrect password. Try again\n");
        free(user_input);
        continue;
      } else if (err == NO_SUCH_USERNAME_ERROR) {
        printf("This user doesn't exists. Try again.\n");
        free(user_input);
        continue;
      } else if (err == SELF_BAN_ERROR) {
        printf("You can not ban yourself.\n");
        free(user_input);
        continue;
      } else if (err != 0) {
        free(user_input);
        return err;
      }
    } else {
      printf("Incorrect command. Try again.\n");
    }

    free(user_input);
  }

  return 0;
}

int handle_time() {
  time_t current_time;
  struct tm *time_info;

  time(&current_time);
  time_info = localtime(&current_time);

  printf("%02d:%02d:%02d\n", time_info->tm_hour, time_info->tm_min,
         time_info->tm_sec);

  return 0;
}
int handle_date() {
  time_t current_time;
  struct tm *time_info;

  time(&current_time);
  time_info = localtime(&current_time);

  printf("%02d:%02d:%d\n", time_info->tm_mday, time_info->tm_mon + 1,
         time_info->tm_year + 1900);

  return 0;
}

int handle_howmuch(char *user_input) {
  if (user_input == NULL) {
    return DEREFERENCING_NULL_ERROR;
  }

  char *time_str = user_input + 8;
  char *flag_str = NULL;
  char *p = time_str;
  struct tm tm_time = {0};
  time_t input_time = 0;
  time_t now = 0;
  double diff = 0;

  while (*p != '\0' && !isspace((unsigned char)*p)) {
    ++p;
  }
  if (*p == '\0') {
    return PARSING_ERROR;
  }

  while (*p != '\0' && isspace((unsigned char)*p)) {
    ++p;
  }
  if (*p == '\0') {
    return PARSING_ERROR;
  }

  flag_str = p;

  p = flag_str;
  while (*p != '\0' && !isspace((unsigned char)*p)) {
    ++p;
  }
  if (*p != '\0') {
    *p = '\0';
    ++p;
  }
  flag_str = p;

  if (sscanf(time_str, "%d:%d:%d %d:%d:%d", &tm_time.tm_mday, &tm_time.tm_mon,
             &tm_time.tm_year, &tm_time.tm_hour, &tm_time.tm_min,
             &tm_time.tm_sec) != 6) {
    return PARSING_ERROR;
  }

  tm_time.tm_year -= 1900;
  tm_time.tm_mon -= 1;

  input_time = mktime(&tm_time);
  if (input_time == -1) {
    return PARSING_ERROR;
  }

  now = time(NULL);
  diff = difftime(now, input_time);

  if (strcmp(flag_str, "-s") == 0) {
    printf("%.0f seconds\n", diff);
  } else if (strcmp(flag_str, "-m") == 0) {
    printf("%.0f minutes\n", diff / 60);
  } else if (strcmp(flag_str, "-h") == 0) {
    printf("%.0f hours\n", diff / 3600);
  } else if (strcmp(flag_str, "-y") == 0) {
    printf("%.2f years\n", diff / (365.25 * 24 * 3600));
  } else {
    return PARSING_ERROR;
  }

  return 0;
}

int handle_sanctions(char *user_input, user_entry const *users, int *users_bans,
                     size_t users_count, unsigned int current_uid) {
  if (user_input == NULL) {
    return DEREFERENCING_NULL_ERROR;
  }

  size_t len = 0;
  size_t i = 0;
  int found = -1;
  char *username = strchr(user_input, ' ');
  if (username == NULL) {
    return PARSING_ERROR;
  }

  while (isspace(*username)) {
    username++;
  }

  len = trim_string(username);

  if (len > 6) {
    return PARSING_ERROR;
  }

  for (i = 0; i < users_count; i++) {
    if (strcmp(users[i].login, username) == 0) {
      found = i;
      break;
    }
  }

  if (found == current_uid - 1) {
    return SELF_BAN_ERROR;
  }

  if (found == -1) {
    return NO_SUCH_USERNAME_ERROR;
  }

  printf("Enter admin password to process: ");
  disable_echo();
  int result = scanf("%zu", &i);
  restore_terminal();
  printf("\n");
  if (i != ADMIN_PASSWORD) {
    return INCORRECT_PASSWORD_ERROR;
  }
  if (result != 1) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    return INCORRECT_PASSWORD_ERROR;
  }

  users_bans[found] = 1;
  printf("Success!\n");

  return 0;
}

int get_users_from_file(user_entry **users, size_t *users_count) {
  FILE *fin;
  long file_size = 0;
  size_t read_users_count = 0;

  fin = fopen(USERS_FILENAME, "rb");
  if (!fin) {
    *users = NULL;
    *users_count = 0;
    return 0;
  }

  if (fseek(fin, 0, SEEK_END) != 0) {
    fclose(fin);
    return FILE_PARSING_ERROR;
  }

  file_size = ftell(fin);
  if (file_size < 0) {
    fclose(fin);
    return FILE_PARSING_ERROR;
  }

  *users_count = file_size / sizeof(user_entry);

  *users = malloc(*users_count * sizeof(user_entry));
  if (!*users) {
    fclose(fin);
    return MEMORY_ALLOCATION_ERROR;
  }

  fseek(fin, 0, SEEK_SET);

  read_users_count = fread(*users, sizeof(user_entry), *users_count, fin);
  if (read_users_count != *users_count) {
    free(*users);
    fclose(fin);
    printf("%zu %zu\n", *users_count, read_users_count);
    return FILE_PARSING_ERROR;
  }

  fclose(fin);
  return 0;
}

int login_user(user_entry const *user) {
  if (user == NULL) {
    return DEREFERENCING_NULL_ERROR;
  }
  unsigned int user_pin = 0;

  printf("Password: ");
  disable_echo();
  int result = scanf("%ud", &user_pin);
  restore_terminal();
  printf("\n");
  if (result != 1) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    return INCORRECT_PASSWORD_ERROR;
  }

  if (user_pin == user->pin) {
    return 0;
  }

  return INCORRECT_PASSWORD_ERROR;
}

int register_user(char const *username, unsigned int *pin_placeholder) {
  if (username == NULL || pin_placeholder == NULL) {
    return DEREFERENCING_NULL_ERROR;
  }
  int user_pin = 0;
  int confirmation = 0;
  printf("To register in shell enter PIN-code number between 0 and 100000: ");
  disable_echo();
  int result = scanf("%d", &user_pin);
  restore_terminal();
  printf("\n");
  if (result != 1) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    return INCORRECT_PASSWORD_ERROR;
  }
  if (user_pin < 0 || user_pin > 100000) {
    *pin_placeholder = 0;
    return INCORRECT_PASSWORD_ERROR;
  }
  printf("Enter PIN-code again for confirmation: ");
  disable_echo();
  result = scanf("%d", &confirmation);
  restore_terminal();
  printf("\n");
  if (result != 1) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    return INCORRECT_PASSWORD_ERROR;
  }
  if (confirmation != user_pin) {
    *pin_placeholder = 0;
    return INCORRECT_PASSWORD_ERROR;
  }

  *pin_placeholder = (unsigned int)user_pin;
  printf("Success!\n");
  return 0;
}

int trim_string(char *s) {
  if (s == NULL) {
    return 0;
  }
  size_t len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    s[len - 1] = '\0';
    --len;
  }
  return len;
}

void print_error(int errno) {
  switch (errno) {
    case MEMORY_ALLOCATION_ERROR:
      printf("Error: Memory allocation failed.\n");
      break;
    case DEREFERENCING_NULL_ERROR:
      printf("Error: Attempted to dereference a null pointer.\n");
      break;
    case PARSING_ERROR:
      printf("Error: Parsing failed.\n");
      break;
    case FILE_PARSING_ERROR:
      printf("Error: Failed to parse the file.\n");
      break;
    case INCORRECT_PASSWORD_ERROR:
      printf("Error: Incorrect password.\n");
      break;
    case FILE_OPENING_ERROR:
      printf("Error: Could not open the file.\n");
      break;
    case NO_SUCH_USERNAME_ERROR:
      printf("Error: Username does not exist.\n");
      break;
    default:
      printf("Error: Unknown error code %d.\n", errno);
      break;
  }
}

void ignore_sigint(int sig) {}

void disable_echo() {
  struct termios t;
  tcgetattr(STDIN_FILENO, &saved_term);
  t = saved_term;
  t.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void restore_terminal() { tcsetattr(STDIN_FILENO, TCSANOW, &saved_term); }

char *command_generator(const char *text, int state) {
  const char *local_commands[] = {"Time",      "Date",   "Howmuch",
                                  "Sanctions", "Logout", NULL};
  static int list_index = 0;
  static int len = 0;
  const char *name = NULL;

  if (!state) {
    list_index = 0;
    len = strlen(text);
  }

  while ((name = local_commands[list_index++])) {
    if (strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }
  return NULL;
}

char **completion(const char *text, int start, int end) {
  rl_attempted_completion_over = 1;

  return rl_completion_matches(text, command_generator);
}
