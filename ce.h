#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#define CE_NEWLINE '\n'
#define CE_TAB '\t'
#define CE_UTF8_INVALID -1
#define CE_UTF8_SIZE 4

#define CE_CLAMP(a, min, max) (a = (a < min) ? min : (a > max) ? max : a);

typedef enum{
     CE_UP = -1,
     CE_DOWN = 1
}CeDirection_t;

typedef enum{
     CE_BUFFER_STATUS_NONE,
     CE_BUFFER_STATUS_MODIFIED,
     CE_BUFFER_STATUS_READONLY,
     CE_BUFFER_STATUS_NEW_FILE,
}CeBufferStatus_t;

typedef enum{
     CE_BUFFER_FILE_TYPE_PLAIN,
     CE_BUFFER_FILE_TYPE_C,
     CE_BUFFER_FILE_TYPE_CPP,
     CE_BUFFER_FILE_TYPE_PYTHON,
     CE_BUFFER_FILE_TYPE_JAVA,
     CE_BUFFER_FILE_TYPE_BASH,
     CE_BUFFER_FILE_TYPE_CONFIG,
     CE_BUFFER_FILE_TYPE_DIFF,
     CE_BUFFER_FILE_TYPE_TERMINAL,
}CeBufferFileType_t;

typedef struct{
     int64_t x;
     int64_t y;
}CePoint_t;

typedef struct{
     int64_t left;
     int64_t right;
     int64_t top;
     int64_t bottom;
}CeRect_t;

typedef struct{
     char** lines;
     int64_t line_count;

     char* name;

     CeBufferStatus_t status;
     CeBufferFileType_t type;

     CePoint_t cursor;

     pthread_mutex_t lock;

     void* user_data;
}CeBuffer_t;

typedef struct{
     CeRect_t rect;
     CePoint_t scroll;

     CePoint_t cursor;

     CeBuffer_t* buffer;

     void* user_data;
}CeView_t;

typedef int32_t CeRune_t;

bool ce_log_init(const char* filename);
void ce_log(const char* fmt, ...);

bool ce_buffer_alloc(CeBuffer_t* buffer, int64_t line_count, const char* name);
void ce_buffer_free(CeBuffer_t* buffer);
bool ce_buffer_load_file(CeBuffer_t* buffer, const char* filename);
bool ce_buffer_load_string(CeBuffer_t* buffer, const char* string, const char* name);
bool ce_buffer_save(CeBuffer_t* buffer);
bool ce_buffer_empty(CeBuffer_t* buffer);

int64_t ce_buffer_range_len(CeBuffer_t* buffer, CePoint_t start, CePoint_t end);
int64_t ce_buffer_line_len(CeBuffer_t* buffer, int64_t line);
CePoint_t ce_buffer_move_point(CeBuffer_t* buffer, CePoint_t point, CePoint_t delta, int64_t tab_width, bool allow_passed_end);
CePoint_t ce_buffer_advance_point(CeBuffer_t* buffer, CePoint_t point, int64_t delta);
bool ce_buffer_contains_point(CeBuffer_t* buffer, CePoint_t point);
int64_t ce_buffer_point_is_valid(CeBuffer_t* buffer, CePoint_t point); // like ce_buffer_contains_point(), but includes end of line as valid

bool ce_buffer_insert_string(CeBuffer_t* buffer, const char* string, CePoint_t point);
bool ce_buffer_remove_string(CeBuffer_t* buffer, CePoint_t point, int64_t length, bool remove_line_if_empty);

bool ce_buffer_insert_char(CeBuffer_t* buffer, char ch, CePoint_t point);

bool ce_buffer_remove_lines(CeBuffer_t* buffer, int64_t line_start, int64_t lines_to_remove);

CePoint_t ce_view_follow_cursor(CeView_t* view, int64_t horizontal_scroll_off, int64_t vertical_scroll_off, int64_t tab_width);

int64_t ce_utf8_strlen(const char* string);
char* ce_utf8_find_index(char* string, int64_t index);
CeRune_t ce_utf8_decode(const char* string, int64_t* bytes_consumed);
bool ce_utf8_encode(CeRune_t u, char* string, int64_t string_len, int64_t* bytes_written);

int64_t ce_util_count_string_lines(const char* string);
int64_t ce_util_string_index_to_visible_index(const char* string, int64_t character, int64_t tab_width);
int64_t ce_util_visible_index_to_string_index(const char* string, int64_t character, int64_t tab_width);
