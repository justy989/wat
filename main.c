#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <locale.h>
#include <sys/time.h>
#include <ncurses.h>

#include "ce.h"

#define DRAW_USEC_LIMIT 16666

typedef struct{
     CeView_t* view;
     int64_t tab_width;
     bool done;
}DrawThreadData_t;

void draw_view(CeView_t* view, int64_t tab_width){
     int64_t view_height = view->rect.bottom - view->rect.top;
     int64_t view_width = view->rect.right - view->rect.left;
     int64_t row_min = view->scroll.y;
     int64_t col_min = view->scroll.x;
     int64_t col_max = col_min + view_width;

     CE_CLAMP(row_min, 0, (view->buffer->line_count - 1));

     for(int64_t y = 0; y < view_height; y++){
          int64_t x = 0;
          int64_t rune_len = 0;
          const char* line = view->buffer->lines[y + row_min];
          CeRune_t rune = 1;

          while(rune > 0){
               rune = ce_utf8_decode(line, &rune_len);
               if(x >= col_min && x <= col_max && rune > 0){
                    int draw_y = view->rect.top + y;
                    int draw_x = view->rect.left + x - col_min;

                    if(rune == CE_TAB){
                         x += tab_width;
                         for(int64_t t = 0; t < tab_width; t++){
                              mvaddch(draw_y, draw_x + t, ' ');
                         }
                    }else{
                         mvaddch(draw_y, draw_x, rune);
                         x++;
                    }
               }else{
                    x++;
               }
               line += rune_len;
          }

          // clear to the end of the view
          x--;
          for(; (x - col_min) <= view_width; x++){
               mvaddch(view->rect.top + y, view->rect.left + x - col_min, ' ');
          }
     }

     move(view->buffer->cursor.y - view->scroll.y + view->rect.top,
          view->buffer->cursor.x - view->scroll.x + view->rect.left);
}

void* draw_thread(void* thread_data){
     DrawThreadData_t* data = (DrawThreadData_t*)thread_data;
     struct timeval previous_draw_time;
     struct timeval current_draw_time;
     uint64_t time_since_last_draw;

     while(!data->done){
          gettimeofday(&previous_draw_time, NULL);

          do{
               gettimeofday(&current_draw_time, NULL);
               time_since_last_draw = (current_draw_time.tv_sec - previous_draw_time.tv_sec) * 1000000LL +
                                      (current_draw_time.tv_usec - previous_draw_time.tv_usec);
          }while(time_since_last_draw < DRAW_USEC_LIMIT);

          standend();
          draw_view(data->view, data->tab_width);
          refresh();
     }

     return NULL;
}

int main(int argc, char** argv){
     setlocale(LC_ALL, "");

     // init ncurses
     {
          initscr();
          keypad(stdscr, TRUE);
          raw();
          cbreak();
          noecho();

          if(has_colors() == FALSE){
               printf("Your terminal doesn't support colors. what year do you live in?\n");
               return -1;
          }

          start_color();
          use_default_colors();
     }

     int64_t tab_width = 8;
     int64_t horizontal_scroll_off = 10;
     int64_t vertical_scroll_off = 5;

     CeBuffer_t buffer = {};
     CeView_t view = {};
     view.buffer = &buffer;
     view.rect.left = 10;
     view.rect.right = 90;
     view.rect.top = 15;
     view.rect.bottom = 50;
     view.scroll.x = 0;
     view.scroll.y = 0;

     if(argc > 1) ce_buffer_load_file(&buffer, argv[1]);

     // init draw thread
     pthread_t thread_draw;
     DrawThreadData_t* draw_thread_data = calloc(1, sizeof(*draw_thread_data));
     {
          draw_thread_data->view = &view;
          draw_thread_data->tab_width = tab_width;
          pthread_create(&thread_draw, NULL, draw_thread, draw_thread_data);
     }

     bool done = false;
     while(!done){
          int key = getch();
          switch(key){
          default:
               break;
          case 'q':
               done = true;
               break;
          case 'h':
               buffer.cursor = ce_buffer_move_point(&buffer, buffer.cursor, (CePoint_t){-1, 0}, tab_width, false);
               ce_view_follow_cursor(&view, horizontal_scroll_off, vertical_scroll_off);
               break;
          case 'j':
               buffer.cursor = ce_buffer_move_point(&buffer, buffer.cursor, (CePoint_t){0, 1}, tab_width, false);
               ce_view_follow_cursor(&view, horizontal_scroll_off, vertical_scroll_off);
               break;
          case 'k':
               buffer.cursor = ce_buffer_move_point(&buffer, buffer.cursor, (CePoint_t){0, -1}, tab_width, false);
               ce_view_follow_cursor(&view, horizontal_scroll_off, vertical_scroll_off);
               break;
          case 'l':
               buffer.cursor = ce_buffer_move_point(&buffer, buffer.cursor, (CePoint_t){1, 0}, tab_width, false);
               ce_view_follow_cursor(&view, horizontal_scroll_off, vertical_scroll_off);
               break;
          }
     }

     draw_thread_data->done = true;
     pthread_cancel(thread_draw);
     pthread_join(thread_draw, NULL);

     // cleanup
     ce_buffer_free(&buffer);
     endwin();
     return 0;
}
