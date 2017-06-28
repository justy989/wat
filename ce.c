#include "ce.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

FILE* g_ce_log = NULL;

bool ce_log_init(const char* filename){
     g_ce_log = fopen(filename, "wa");
     if(!g_ce_log){
          fprintf(stderr, "error: unable to create ce log: fopen(\"%s\", \"wa\") failed: '%s'\n", filename, strerror(errno));
          return false;
     }

     static const char* greetings [] = {
          "Thank you for flying ce",
          "There's nothing like a fresh cup of ce in the morning",
          "Why do kids love the taste of C Editor?\n\nIt's the taste you can ce",
          "ce is for C Editor, that's good enough for me",
          "I missed you.",
          "Hope you're having a great day! -ce",
          "You're a special person -- or robot. I don't judge.",
          "I missed you... in a creepy way.",
          "I'm a potato",
          "At least this isn't emacs? Am I right!",
          "TACOCAT is the best palindrome",
          "Found a bug? It's a feature.",
          "Yo.",
          "Slurp'n up whitespace since 2016",
          "Welcome to GNU Emacs, one component of the GNU/Linux operating system.",
          "ce, the world's only editor with a Michelin star.",
          "Oy! ce's a beaut!",
          "The default config has a great vimplementation!",
          "They see me slurpin' They hatin'",
          "'Days of pain are worth the years of upcoming prosperity' -confucius, probably",
          "ce, aka, 'the cache miss king'",
          "All the terminal you want with none of the illness",
          "I used ce before it was cool",
          "'Where has this been all my life' -emacs enthusiast",
     };

     srand(time(NULL));
     ce_log("%s\n", greetings[rand() % (sizeof(greetings) / sizeof(greetings[0]))]);
     return true;
}

void ce_log(const char* fmt, ...){
     va_list args;
     va_start(args, fmt);
     vfprintf(g_ce_log, fmt, args);
     va_end(args);
}

static bool buffer_realloc_lines(CeBuffer_t* buffer, int64_t new_line_count){
     char** old_ptr = buffer->lines;
     buffer->lines = realloc(buffer->lines, new_line_count * sizeof(buffer->lines[0]));
     if(buffer->lines == old_ptr && new_line_count > buffer->line_count) return false;
     buffer->line_count = new_line_count;
     return true;
}

bool ce_buffer_alloc(CeBuffer_t* buffer, int64_t line_count, const char* name){
     if(buffer->lines) ce_buffer_free(buffer);

     if(line_count <= 0){
          ce_log("%s() error: 0 line_count specified.\n", __FUNCTION__);
          return false;
     }

     buffer->lines = (char**)malloc(line_count * sizeof(*buffer->lines));
     if(!buffer->lines){
          ce_log("%s() failed to malloc() %ld lines.\n", __FUNCTION__, line_count);
          return false;
     }

     buffer->line_count = line_count;
     buffer->name = strdup(name);

     for(int64_t i = 0; i < line_count; i++){
          buffer->lines[i] = (char*)calloc(1, sizeof(buffer->lines[i]));
     }

     buffer->status = CE_BUFFER_STATUS_MODIFIED;
     pthread_mutex_init(&buffer->lock, NULL);
     return true;
}

void ce_buffer_free(CeBuffer_t* buffer){
     for(int64_t i = 0; i < buffer->line_count; i++){
          free(buffer->lines[i]);
     }

     free(buffer->lines);
     free(buffer->name);

     pthread_mutex_destroy(&buffer->lock);

     memset(buffer, 0, sizeof(*buffer));
}

bool ce_buffer_load_file(CeBuffer_t* buffer, const char* filename){
     // read the entire file
     size_t content_size;
     char* contents = NULL;

     FILE* file = fopen(filename, "rb");
     if(!file){
          ce_log("%s() fopen('%s', 'rb') failed: '%s'\n", __FUNCTION__, filename, strerror(errno));
          return true;
     }

     fseek(file, 0, SEEK_END);
     content_size = ftell(file);
     fseek(file, 0, SEEK_SET);

     contents = (char*)malloc(content_size + 1);
     fread(contents, content_size, 1, file);
     contents[content_size] = 0;

     // strip the ending '\n'
     if(contents[content_size - 1] == CE_NEWLINE) contents[content_size - 1] = 0;

     if(!ce_buffer_load_string(buffer, contents, filename)){
          return false;
     }

     fclose(file);

     if(access(filename, W_OK) != 0){
          buffer->status = CE_BUFFER_STATUS_READONLY;
     }else{
          buffer->status = CE_BUFFER_STATUS_NONE;
     }

     free(contents);
     ce_log("%s() loaded '%s'\n", __FUNCTION__, filename);
     return true;
}

bool ce_buffer_load_string(CeBuffer_t* buffer, const char* string, const char* name){
     if(buffer->lines) ce_buffer_free(buffer);

     int64_t line_count = ce_util_count_string_lines(string);

     // allocate for the number of lines contained in the string
     buffer->lines = (char**)malloc(line_count * sizeof(*buffer->lines));
     if(!buffer->lines){
          ce_log("%s() failed to allocate %ld lines\n", __FUNCTION__, line_count);
          return false;
     }

     buffer->line_count = line_count;
     buffer->name = strdup(name);

     // loop over each line
     const char* newline = NULL;
     size_t line_len = 0;
     for(int64_t i = 0; i < line_count; i++){
          // look for the newline
          newline = strchr(string, CE_NEWLINE);

          if(newline){
               // allocate space for the line, excluding the newline character
               line_len = newline - string;
               buffer->lines[i] = (char*)malloc(line_len + 1);
               buffer->lines[i] = strncpy(buffer->lines[i], string, line_len);
               buffer->lines[i][line_len] = 0;

               string = newline + 1;
          }else{
               // if this is the end, just dupe it
               buffer->lines[i] = strdup(string);
               break;
          }
     }

     pthread_mutex_init(&buffer->lock, NULL);
     return true;
}

bool ce_buffer_empty(CeBuffer_t* buffer){
     if(buffer->lines == NULL) return false;

     // re allocate it down to a single blank line
     buffer->lines = realloc(buffer->lines, sizeof(*buffer->lines));
     buffer->lines[0] = realloc(buffer->lines[0], sizeof(buffer->lines[0]));
     buffer->lines[0][0] = 0;
     buffer->line_count = 1;

     return true;
}

int64_t ce_buffer_contains_point(CeBuffer_t* buffer, CePoint_t point){
     if(point.y < 0 || point.y >= buffer->line_count || point.x < 0) return false;
     int64_t line_len = ce_utf8_strlen(buffer->lines[point.y]);
     if(point.x >= line_len){
          if(line_len == 0 && point.x == 0){
               return true;
          }
          return false;
     }

     return true;
}

int64_t ce_buffer_line_len(CeBuffer_t* buffer, int64_t line){
     if(line < 0 || line > buffer->line_count) return -1;

     return ce_utf8_strlen(buffer->lines[line]);
}

CePoint_t ce_buffer_move_point(CeBuffer_t* buffer, CePoint_t point, CePoint_t delta, int64_t tab_width, bool allow_passed_end){
     if(!ce_buffer_contains_point(buffer, point)) return point;

     if(delta.y){
          // figure out where we are visibly (due to tabs being variable length)
          int64_t cur_visible_index = ce_util_string_index_to_visible_index(buffer->lines[point.y], point.x, tab_width);

          // move to the new line
          point.y += delta.y;
          CE_CLAMP(point.y, 0, (buffer->line_count - 1));

          // convert the x from visible index to a string index
          point.x = ce_util_visible_index_to_string_index(buffer->lines[point.y], cur_visible_index, tab_width);
     }

     point.x += delta.x;
     int64_t line_len = ce_buffer_line_len(buffer, point.y);

     if(allow_passed_end){
          CE_CLAMP(point.x, 0, line_len);
     }else{
          if(line_len == 0){
               point.x = 0;
          }else{
               CE_CLAMP(point.x, 0, (line_len - 1));
          }
     }

     return point;
}

bool ce_buffer_insert_string(CeBuffer_t* buffer, const char* string, CePoint_t point){
     if(!ce_buffer_contains_point(buffer, point)) return false;

     int64_t string_lines = ce_util_count_string_lines(string);
     if(string_lines == 0){
          return true; // sure, yeah, we inserted that empty string
     }else if(string_lines == 1){
          pthread_mutex_lock(&buffer->lock);

          size_t insert_len = strlen(string);
          size_t existing_len = strlen(buffer->lines[point.y]);
          size_t total_len = insert_len + existing_len;

          // re-alloc the new size
          char* line = buffer->lines[point.y];
          line = realloc(line, total_len + 1);
          if(!line){
               pthread_mutex_unlock(&buffer->lock);
               return false;
          }

          // figure out where to move from and to
          char* src = ce_utf8_find_index(line, point.x);
          char* dst = src + insert_len;
          size_t src_len = strlen(src);
          memmove(dst, src, src_len);

          // insert the string
          memcpy(src, string, insert_len);

          // tidy up
          line[total_len] = 0;
          buffer->lines[point.y] = line;
          pthread_mutex_unlock(&buffer->lock);
          return true;
     }

     int64_t shift_lines = string_lines - 1;
     int64_t old_line_count = buffer->line_count;

     // allocate space to fit the buffer plus the new multiline string
     if(!buffer_realloc_lines(buffer, buffer->line_count + shift_lines)){
          return false;
     }

     // shift down all the line pointers
     int64_t first_new_line = point.y + 1;
     char** src_line = buffer->lines + first_new_line;
     char** dst_line = src_line + shift_lines;
     size_t move_count = old_line_count - first_new_line;
     memmove(dst_line, src_line, move_count * sizeof(src_line));

     // save the last part of the first line to stick on the end of the multiline string
     char* end_string = NULL;
     int64_t end_string_len = strlen(buffer->lines[point.y] + point.x);
     if(end_string_len) end_string = strdup(buffer->lines[point.y] + point.x);

     // insert the first line of the string at the point specified
     const char* next_newline = strchr(string, '\n');
     assert(next_newline);
     size_t first_line_len = next_newline - string;
     size_t new_line_len = point.x + first_line_len;
     buffer->lines[point.y] = realloc(buffer->lines[point.y], new_line_len);
     memcpy(buffer->lines[point.y] + point.x, string, first_line_len);
     buffer->lines[point.y][new_line_len] = 0;

     // copy in each of the new lines
     string = next_newline + 1;
     next_newline = strchr(string, '\n');
     int64_t next_line = point.y + 1;
     while(next_newline){
          new_line_len = next_newline - string;
          buffer->lines[next_line] = calloc(1, new_line_len + 1);
          memcpy(buffer->lines[next_line], string, new_line_len);
          buffer->lines[next_line][new_line_len] = 0;
          string = next_newline + 1;
          next_newline = strchr(string, '\n');
          next_line++;
     }

     // copy in the last line
     new_line_len = strlen(string);
     int64_t last_line_len = new_line_len + end_string_len;
     buffer->lines[next_line] = calloc(1, last_line_len);
     memcpy(buffer->lines[next_line], string, new_line_len);

     // attach the end part of the line we inserted into at the end of the last line
     if(end_string) memcpy(buffer->lines[next_line] + new_line_len, end_string, end_string_len);
     buffer->lines[next_line][last_line_len] = 0;

     return true;
}

bool ce_buffer_remove_string(CeBuffer_t* buffer, CePoint_t point, int64_t length, bool remove_line_if_empty){
     (void)(remove_line_if_empty);
     if(!ce_buffer_contains_point(buffer, point)) return false;

     int64_t len_left_on_line = ce_utf8_strlen(buffer->lines[point.y] + point.x);
     if(len_left_on_line > length){
          char* end_of_start = ce_utf8_find_index(buffer->lines[point.y], point.x);
          assert(end_of_start);
          char* beginning_of_end = ce_utf8_find_index(buffer->lines[point.y], point.x + length);
          assert(beginning_of_end);

          // figure out how big of a line to allocate
          size_t start_line_len = end_of_start - buffer->lines[point.y];
          size_t end_line_len = strlen(beginning_of_end);
          size_t full_line_len = start_line_len + end_line_len;
          char* new_line = calloc(full_line_len + 1, sizeof(*new_line));
          if(!new_line) return false;

          // copy over the data to our new line
          memcpy(new_line, buffer->lines[point.y], start_line_len);
          memcpy(new_line + start_line_len, beginning_of_end, end_line_len);
          new_line[full_line_len] = 0;

          // free and overwrite our new line
          free(buffer->lines[point.y]);
          buffer->lines[point.y] = new_line;

          return true;
     }else if(len_left_on_line == length){
          if(point.x == 0 && remove_line_if_empty){
               // remove the empty line from the buffer lines
               char** dst_line = buffer->lines + point.y;
               char** src_line = dst_line + 1;
               int64_t lines_to_shift = buffer->line_count - (point.y + 1);
               free(buffer->lines[point.y]);
               memmove(dst_line, src_line, lines_to_shift);
               if(!buffer_realloc_lines(buffer, buffer->line_count - 1)){
                    return false;
               }
          }else{
               // re-alloc for just the first part of the line, even if it is 0 len
               buffer->lines[point.y] = realloc(buffer->lines[point.y], point.x + 1);
               buffer->lines[point.y][point.x] = 0;
          }

          return true;
     }

     // how many lines do we have to delete?
     int64_t length_itr = length - len_left_on_line;
     int64_t current_line;
     int64_t line_len = 0;

     for(current_line = point.y + 1; length_itr > 0 && current_line < buffer->line_count; current_line++){
          line_len = ce_utf8_strlen(buffer->lines[current_line]);
          length_itr -= line_len;
     }

     // int64_t lines_to_delete = current_line - point.y;

     current_line--;

     char* beginning_of_end = ce_utf8_find_index(buffer->lines[current_line], line_len + length_itr);
     assert(beginning_of_end);
     char* end_of_start = ce_utf8_find_index(buffer->lines[point.y], point.x);
     assert(end_of_start);
     int64_t start_len = end_of_start - buffer->lines[point.y];
     int64_t end_len = strlen(beginning_of_end);
     int64_t new_first_line_len = start_len + end_len;
     char* new_first_line = malloc(new_first_line_len + 1);
     memcpy(new_first_line, buffer->lines[point.y], start_len);
     memcpy(new_first_line + start_len, beginning_of_end, end_len);
     new_first_line[new_first_line_len] = 0;

     return false;
}

CePoint_t ce_view_follow_cursor(CeView_t* view, int64_t horizontal_scroll_off, int64_t vertical_scroll_off, int64_t tab_width){
     CePoint_t delta = view->scroll;

     if(!view->buffer) return delta;

     int64_t scroll_left = view->scroll.x + horizontal_scroll_off;
     int64_t scroll_top = view->scroll.y + vertical_scroll_off;
     int64_t scroll_right = view->scroll.x + (view->rect.right - view->rect.left) - horizontal_scroll_off;
     int64_t scroll_bottom = view->scroll.y + (view->rect.bottom - view->rect.top) - vertical_scroll_off;

     int64_t visible_index = ce_util_string_index_to_visible_index(view->buffer->lines[view->cursor.y],
                                                                   view->cursor.x, tab_width);

     pthread_mutex_lock(&view->buffer->lock);

     if(visible_index < scroll_left){
          view->scroll.x -= (scroll_left - visible_index);
          if(view->scroll.x < 0) view->scroll.x = 0;
     }else if(visible_index > scroll_right){
          view->scroll.x += (visible_index - scroll_right);
     }

     if(view->cursor.y < scroll_top){
          view->scroll.y -= (scroll_top - view->cursor.y);
          if(view->scroll.y < 0) view->scroll.y = 0;
     }else if(view->cursor.y > scroll_bottom){
          view->scroll.y += (view->cursor.y - scroll_bottom);
     }

     pthread_mutex_unlock(&view->buffer->lock);

     delta.x = view->scroll.x - delta.x;
     delta.y = view->scroll.y - delta.y;

     return delta;
}

int64_t ce_utf8_strlen(const char* string){
     int64_t len = 0;
     int64_t byte_count = 0;

     while(*string){
          if((*string & 0x80) == 0){
               byte_count = 1;
          }else if((*string & 0xE0) == 0xC0){
               byte_count = 2;
          }else if((*string & 0xF0) == 0xE0){
               byte_count = 3;
          }else if((*string & 0xF8) == 0xF0){
               byte_count = 4;
          }else{
               return -1;
          }

          // validate string doesn't early terminate
          for(int64_t i = 0; i < byte_count; i++){
               if(*string == 0) return -1;
               string++;
          }

          len++;
     }

     return len;
}

char* ce_utf8_find_index(char* string, int64_t index){
     int64_t bytes = 0;
     while(index){
          if((*string & 0x80) == 0){
               bytes = 1;
          }else if((*string & 0xE0) == 0xC0){
               bytes = 2;
          }else if((*string & 0xF0) == 0xE0){
               bytes = 3;
          }else if((*string & 0xF8) == 0xF0){
               bytes = 4;
          }else{
               return NULL;
          }

          for(int64_t i = 0; i < bytes; ++i){
               if(*string == 0) return NULL;
               string++;
          }

          index--;
     }

     return string;
}

CeRune_t ce_utf8_decode(const char* string, int64_t* bytes_consumed){
     CeRune_t rune;

     // 0xxxxxxx is just ascii
     if((*string & 0x80) == 0){
          *bytes_consumed = 1;
          rune = string[0];
     // 110xxxxx is a 2 byte utf8 string
     }else if((*string & 0xE0) == 0xC0){
          *bytes_consumed = 2;
          rune = string[0] & 0x1F;
          rune <<= 6;
          rune |= string[1] & 0x3F;
     // 1110xxxx is a 3 byte utf8 string
     }else if((*string & 0xF0) == 0xE0){
          *bytes_consumed = 3;
          rune = string[0] & 0x0F;
          rune <<= 6;
          rune |= string[1] & 0x3F;
          rune <<= 6;
          rune |= string[2] & 0x3F;
     // 11110xxx is a 4 byte utf8 string
     }else if((*string & 0xF8) == 0xF0){
          *bytes_consumed = 4;
          rune = string[0] & 0x0F;
          rune <<= 6;
          rune |= string[1] & 0x3F;
          rune <<= 6;
          rune |= string[2] & 0x3F;
          rune <<= 6;
          rune |= string[3] & 0x3F;
     }else{
          return CE_UTF8_INVALID;
     }

     return rune;
}

bool ce_utf8_encode(CeRune_t u, char* string, int64_t string_len, int64_t* bytes_written){
     if(u < 0x80){
          if(string_len < 1) return false;
          *bytes_written = 1;

          // leave as-is
          string[0] = u;
     }else if(u < 0x0800){
          if(string_len < 2) return false;
          *bytes_written = 2;

          // u = 00000000 00000000 00000abc defghijk

          // 2 bytes
          // first byte:  110abcde
          string[0] = 0xC0 | ((u >> 6) & 0x1f);

          // second byte: 10fghijk
          string[1] = 0x80 | (u & 0x3f);
     }else if(u < 0x10000){
          if(string_len < 3) return false;
          *bytes_written = 3;

          // u = 00000000 00000000 abcdefgh ijklmnop

          // 3 bytes
          // first byte:  1110abcd
          string[0] = 0xE0 | ((u >> 12) & 0x0F);

          // second byte: 10efghij
          string[1] = 0x80 | ((u >> 6) & 0x3F);

          // third byte:  10klmnop
          string[2] = 0x80 | (u & 0x3F);
     }else if(u < 0x110000){
          if(string_len < 4) return false;
          *bytes_written = 4;

          // u = 00000000 000abcde fghijklm nopqrstu

          // 4 bytes
          // first byte:  11110abc
          string[0] = 0xF0 | ((u >> 18) & 0x07);
          // second byte: 10defghi
          string[1] = 0x80 | ((u >> 12) & 0x3F);
          // third byte:  10jklmno
          string[2] = 0x80 | ((u >> 6) & 0x3F);
          // fourth byte: 10pqrstu
          string[3] = 0x80 | (u & 0x3F);
     }

     return true;
}

int64_t ce_util_count_string_lines(const char* string){
     int64_t string_length = strlen(string);
     int64_t line_count = 0;
     for(int64_t i = 0; i <= string_length; ++i){
          if(string[i] == CE_NEWLINE || string[i] == 0) line_count++;
     }

     // TODO: do we need this?
     // one line files usually contain newlines at the end
     // if(line_count == 2 && string[string_length-1] == CE_NEWLINE){
          // line_count--;
     // }

     return line_count;
}

int64_t ce_util_string_index_to_visible_index(const char* string, int64_t index, int64_t tab_width){
     int64_t x = 0;
     int64_t rune_len = 0;
     CeRune_t rune = 1;

     while(rune > 0 && index > 0){
          rune = ce_utf8_decode(string, &rune_len);

          if(rune > 0){
               if(rune == CE_TAB){
                    x += tab_width;
               }else{
                    x++;
               }
          }else{
               x++;
          }

          string += rune_len;
          index--;
     }

     return x;
}

int64_t ce_util_visible_index_to_string_index(const char* string, int64_t index, int64_t tab_width){
     int64_t x = 0;
     int64_t rune_len = 0;
     CeRune_t rune = 1;

     while(rune > 0 && index > 0){
          rune = ce_utf8_decode(string, &rune_len);

          if(rune > 0){
               if(rune == CE_TAB){
                    index -= tab_width;
               }else{
                    index--;
               }
          }else{
               index--;
          }

          string += rune_len;
          x++;
     }

     return x;
}
