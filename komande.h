#pragma once

#include "main.h"
#include <ui/ui.h>
#include <ui/window.h>
#include <ui/filedialog.h>
#include <ui/promptdialog.h>


/* Postojeći prototipovi kontrolnih dugmadi */
void btn_prev_clicked(ui_pbutton_t *button, void *arg);
void btn_next_clicked(ui_pbutton_t *button, void *arg);
void btn_l_ucitaj_clicked(ui_pbutton_t *button, void *arg);
void btn_l_pesma_clicked(ui_pbutton_t *button, void *arg);
void btn_l_osvezi_clicked(ui_pbutton_t *button, void *arg);
void btn_l_obrisi_clicked(ui_pbutton_t *button, void *arg);

/* Callback-ovi za File Dialog (Dugme PESMA) */
void muzika_file_dialog_bok(ui_file_dialog_t *dialog, void *arg, const char *fname);
void muzika_file_dialog_bcancel(ui_file_dialog_t *dialog, void *arg);
void muzika_file_dialog_close(ui_file_dialog_t *dialog, void *arg);

/* Callback-ovi za Prompt Dialog (Dugme UCITAJ - Direktorijum) */
void muzika_prompt_dialog_bok(ui_prompt_dialog_t *dialog, void *arg, const char *text);
void muzika_prompt_dialog_bcancel(ui_prompt_dialog_t *dialog, void *arg);
void muzika_prompt_dialog_close(ui_prompt_dialog_t *dialog, void *arg);


void btn_stop_clicked(ui_pbutton_t *button, void *arg);
void btn_pause_clicked(ui_pbutton_t *button, void *arg);
void btn_playlist_clicked(ui_pbutton_t *button, void *arg);

extern ui_pbutton_cb_t prev_cb;
extern ui_pbutton_cb_t next_cb;
extern ui_pbutton_cb_t l_ucitaj_cb;
extern ui_pbutton_cb_t l_pesma_cb;
extern ui_pbutton_cb_t l_osvezi_cb;
extern ui_pbutton_cb_t l_obrisi_cb;
