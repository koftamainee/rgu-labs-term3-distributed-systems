#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_prime_byte(unsigned char b) {
  if (b < 2) {
    return 0;
  }
  for (int i = 2; i * i <= b; i++) {
    if (b % i == 0) {
      return 0;
    }
  }
  return 1;
}

int main(int argc, char *argv[]) {
  FILE *fin = NULL;
  if (argc < 3) {
    fprintf(stderr, "incorrect usage\n");
    return 1;
  }

  fin = fopen(argv[1], "rb");
  if (fin == NULL) {
    fprintf(stderr, "failed to open a file\n");
    return 1;
  }

  if (strcmp(argv[2], "xor8") == 0) {
    unsigned char byte, result = 0;
    while (fread(&byte, 1, 1, fin) == 1) {
      result ^= byte;
    }
    printf("%u\n", result);
  } else if (strcmp(argv[2], "xorodd") == 0) {
    unsigned int val, result = 0;
    while (fread(&val, 4, 1, fin) == 1) {
      unsigned char *bytes = (unsigned char *)&val;
      int has_prime = 0;
      for (int i = 0; i < 4; i++) {
        if (is_prime_byte(bytes[i])) {
          has_prime = 1;
          break;
        }
      }
      if (has_prime) {
        result ^= val;
      }
    }
    printf("%u\n", result);
  } else if (strcmp(argv[2], "mask") == 0) {
    if (argc < 4) {
      fprintf(stderr, "mask mode requires hex mask argument\n");
      fclose(fin);
      return 1;
    }
    unsigned int mask = (unsigned int)strtoul(argv[3], NULL, 16);
    unsigned int val;
    unsigned int count = 0;
    while (fread(&val, 4, 1, fin) == 1) {
      if ((val & mask) == mask) {
        count++;
      }
    }
    printf("%u\n", count);
  } else {
    fprintf(stderr, "Invalid flag\n");
    fclose(fin);
    return 1;
  }

  fclose(fin);
  return 0;
}
