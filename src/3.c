#include <stdio.h>

int main(int argc, char *argv[]) {
  unsigned char data[BUFSIZ];
  FILE *fin = NULL;
  FILE *fout = NULL;
  size_t bytes_read = 0;

  if (argc != 3) {
    fprintf(stderr, "Incorrect usage\n");
  }

  fin = fopen(argv[1], "rb");
  if (fin == NULL) {
    fprintf(stderr, "Error opening a file\n");
    return 1;
  }

  fout = fopen(argv[2], "wb");
  if (fin == NULL) {
    fprintf(stderr, "Error opening a file\n");
    fclose(fin);
    return 1;
  }

  do {
    bytes_read = fread(data, 1, BUFSIZ, fin);
    fwrite(data, 1, bytes_read, fout);
  } while (bytes_read != 0);

  fclose(fin);
  fclose(fout);
}
