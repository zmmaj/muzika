#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <ui/ui.h>
#include <ui/window.h>
#include <ui/fixed.h>
#include <ui/entry.h>
#include <ui/pbutton.h>
#include <ui/list.h>
#include <ui/menubar.h>   
#include <ui/menu.h>      
#include <ui/menudd.h>    
#include <ui/menuentry.h> 
#include <gfx/coord.h>

#define NAME "muzika"
#define MAX_PESAMA 64

typedef struct {
	gfx_rect_t display_rect;
	gfx_coord2_t btn_orig;
	gfx_coord2_t btn_stride;
	gfx_coord2_t btn_dim;
	gfx_rect_t list_rect;
} muzika_geom_t;

typedef struct {
	ui_t *ui;
	ui_window_t *window;
	ui_fixed_t *fixed;
	ui_entry_t *display;
	ui_list_t *playlist_view;
	
	ui_list_entry_t *list_entries[MAX_PESAMA];
	
	// Gornji red dugmadi
	ui_pbutton_t *btn_prev;     
	ui_pbutton_t *btn_play;
	ui_pbutton_t *btn_pause;
	ui_pbutton_t *btn_stop;
	ui_pbutton_t *btn_next;     
	ui_pbutton_t *btn_playlist; 
	
	// Nova uspravna dugmad sa desne strane liste
	ui_pbutton_t *btn_l_ucitaj; 
	ui_pbutton_t *btn_l_pesma;  
	ui_pbutton_t *btn_l_osvezi; 
	ui_pbutton_t *btn_l_obrisi; 
	
	// Elementi menija na vrhu prozora
	ui_menu_bar_t *menu_bar;    
	ui_menu_t *menu_file;       
	ui_menu_t *menu_help;       
	ui_menu_entry_t *me_exit;   
	ui_menu_entry_t *me_about;  
	
	muzika_geom_t geom;
} muzika_t;

void btn_play_clicked(ui_pbutton_t *button, void *arg);
void skeniraj_direktorijum(const char *putanja);
