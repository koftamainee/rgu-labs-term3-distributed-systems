#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_prime_byte(unsigned char b) {
  static const unsigned char prime_table[256] = {
      [2] = 1,   [3] = 1,   [5] = 1,   [7] = 1,   [11] = 1,  [13] = 1,
      [17] = 1,  [19] = 1,  [23] = 1,  [29] = 1,  [31] = 1,  [37] = 1,
      [41] = 1,  [43] = 1,  [47] = 1,  [53] = 1,  [59] = 1,  [61] = 1,
      [67] = 1,  [71] = 1,  [73] = 1,  [79] = 1,  [83] = 1,  [89] = 1,
      [97] = 1,  [101] = 1, [103] = 1, [107] = 1, [109] = 1, [113] = 1,
      [127] = 1, [131] = 1, [137] = 1, [139] = 1, [149] = 1, [151] = 1,
      [157] = 1, [163] = 1, [167] = 1, [173] = 1, [179] = 1, [181] = 1,
      [191] = 1, [193] = 1, [197] = 1, [199] = 1, [211] = 1, [223] = 1,
      [227] = 1, [229] = 1, [233] = 1, [239] = 1, [241] = 1, [251] = 1};
  return prime_table[b];
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
    printf("%X\n", result);
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
