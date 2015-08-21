// (C) Copyright 2015 by Wade L. Hennessey. All rights reserved.

int SXupdate_visual_page(int page_number);
void SXmaybe_update_visual_page(int page_number, int old_bytes_used, int new_bytes_used);
void SXupdate_visual_static_page(int page_number);
void SXupdate_visual_fake_ptr_page(int page_index);
void SXdraw_visual_gc_state(void);
void SXdraw_visual_gc_stats(void);
void SXvisual_runbar_on(void);
void SXvisual_runbar_off(void);
void SXinit_visual_memory(void);
void SXmousedown_in_visual_memory_window(int width, int height, short modifiers);
void SXmouseup_in_visual_memory_window(int width, int height, short modifiers);
void SXrefresh_visual_memory_window(void);
void SXtoggle_visual_memory(void);
void SXterminate_visual_memory(void);
