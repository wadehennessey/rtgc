// (C) Copyright 2015 - 2016 by Wade L. Hennessey. All rights reserved.

int RTupdate_visual_page(int page_number);
void RTmaybe_update_visual_page(int page_number, int old_bytes_used, int new_bytes_used);
void RTupdate_visual_static_page(int page_number);
void RTupdate_visual_fake_ptr_page(int page_index);
void RTdraw_visual_gc_state(void);
void RTdraw_visual_gc_stats(void);
void RTvisual_runbar_on(void);
void RTvisual_runbar_off(void);
void RTinit_visual_memory(void);
void RTmousedown_in_visual_memory_window(int width, int height, short modifiers);
void RTmouseup_in_visual_memory_window(int width, int height, short modifiers);
void RTrefresh_visual_memory_window(void);
void RTtoggle_visual_memory(void);
void RTterminate_visual_memory(void);
