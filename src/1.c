#include <stdio.h>

void dump_file_struct(FILE *f);

int main(int argc, char *argv[]) {
  unsigned char byte = 0;
  unsigned char buffer[4];
  FILE *fin = NULL;
  FILE *to_write = NULL;
  int i = 0;
  unsigned char data[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};

  if (argc != 2) {
    fprintf(stderr, "incorrect cli args\n");
    return 1;
  }

  to_write = fopen(argv[1], "wb");
  if (to_write == NULL) {
    fprintf(stderr, "error opening the file\n");
    return 1;
  }

  fwrite(data, sizeof(unsigned char), sizeof(data), to_write);

  fclose(to_write);

  fin = fopen(argv[1], "rb");
  if (fin == NULL) {
    fprintf(stderr, "error opening the file\n");
    return 1;
  }

  while (fread(&byte, 1, 1, fin) == 1) {
    printf("Byte: %u\n", byte);
    dump_file_struct(fin);
  }

  fclose(fin);
  fin = NULL;

  fin = fopen(argv[1], "rb");
  if (fin == NULL) {
    fprintf(stderr, "error opening the file\n");
    return 1;
  }

  fseek(fin, 3, SEEK_SET);
  fread(buffer, 1, 4, fin);

  printf("Buffer content:");
  for (i = 0; i < 4; i++) {
    printf(" %u", buffer[i]);
    if (i < 3) {
      printf(",");
    }
  }
  printf("\n");
}

void dump_file_struct(FILE *f) {
  printf("_IO_write_base:  %s\n", f->_IO_write_base);
  printf("_IO_write_ptr:   %p\n", f->_IO_write_ptr);
  printf("_IO_write_end:   %s\n", f->_IO_write_end);

  printf("_IO_read_base:   %s\n", f->_IO_read_base);
  printf("_IO_read_ptr:    %p\n", f->_IO_read_ptr);
  printf("_IO_read_end:    %s\n", f->_IO_read_end);

  printf("_IO_buf_base:    %s\n", f->_IO_buf_base);
  printf("_IO_buf_end:     %s\n", f->_IO_buf_end);

  printf("_IO_save_base:   %s\n", f->_IO_save_base);
  printf("_IO_save_end:    %s\n", f->_IO_save_end);
  printf("_IO_backup_base: %s\n", f->_IO_backup_base);

  printf("_offset:         %ld\n", f->_offset);
  printf("_old_offset:     %ld\n", f->_old_offset);

  printf("_fileno:         %d\n", f->_fileno);
  printf("_mode:           %d\n", f->_mode);
  printf("_flags:          %d\n", f->_flags);
  printf("_flags2:         %d\n", f->_flags2);

  printf("_shortbuf:       %d\n", *f->_shortbuf);
  printf("_short_backupbuf:%d\n", *f->_short_backupbuf);

  printf("_total_written:  %lu\n", f->_total_written);
  printf("_cur_column:     %hu\n", f->_cur_column);

  printf("_lock:           %p\n", f->_lock);
  printf("_markers:        %p\n", f->_markers);
  printf("_codecvt:        %p\n", f->_codecvt);
  printf("_wide_data:      %p\n", f->_wide_data);

  printf("_chain:          %p\n", f->_chain);
  printf("_prevchain:      %p\n", f->_prevchain);
  printf("_freeres_buf:    %p\n", f->_freeres_buf);
  printf("_freeres_list:   %p\n", f->_freeres_list);

  printf("_vtable_offset:  %hhd\n", f->_vtable_offset);
  printf("_unused2:        %d, %d\n", f->_unused2[0], f->_unused2[1]);
  printf("_unused3:        %d\n", f->_unused3);

  printf("\n");
}
