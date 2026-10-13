#include <fibril.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>
#include <errno.h>
#include <dirent.h>
#include "main.h"
#include "meni.h"
#include "komande.h"
#include "audio.h"


static const char srbin_verzija[] = "SRBINOS_VER:2.0.0";
const char *__attribute__((used)) srbin_verzija_ptr = srbin_verzija;

char plejlista[MAX_PESAMA][128];
int ukupan_broj_pesama = 0;
int trenutni_indeks = 0;

// Geometrija prilagođena za širi prozor od 440px i 6 dugmadi
static muzika_geom_t muzika_geom_default = {
	.display_rect = { {15, 48}, {425, 78} },   // Puna širina za status ispisa
	.btn_orig     = { 15, 95 },                // Početak za gornji red dugmadi
	.btn_stride   = { 68, 0 },                 // Razmak za standardna dugmad
	.btn_dim      = { 64, 32 },                // Standardna dimenzija (PREV, PLAY, PAUZA, STOP, NEXT)
	.list_rect    = { {15, 145}, {320, 340} }  // SUŽENA LISTA (ostavlja 105px prostora desno!)
};

static muzika_t *glavna_muzika_instanca = NULL;

// Formatiranje vremena (MM:SS) bez %s i snprintf komplikacija
static void formatiraj_vreme(char *dest, size_t max_size, int sekunde)
{
	int min = sekunde / 60;
	int sec = sekunde % 60;
	// Koristimo bazični snprintf samo za cele brojeve (nema format-truncation provera za %d)
	snprintf(dest, max_size, "%02d:%02d", min, sec);
}




// --- FUNKCIJA KOJA ISPISUJE VREME NA EKRANU PLEJERA U REALNOM VREMENU ---
void azurirajte_vreme_u_gui(void)
{
	if (!glavna_muzika_instanca || !glavna_muzika_instanca->display) return;

	char t_str[16];
	char u_str[16];
	formatiraj_vreme(t_str, sizeof(t_str), trenutna_sekunda);
	formatiraj_vreme(u_str, sizeof(u_str), ukupno_sekundi);

	char poruka[256];
	str_cpy(poruka, sizeof(poruka), "Slusate: ");
	str_lcat(poruka, trenutni_fajl, sizeof(poruka));
	str_lcat(poruka, " (", sizeof(poruka));
	str_lcat(poruka, t_str, sizeof(poruka));
	str_lcat(poruka, " / ", sizeof(poruka));
	str_lcat(poruka, u_str, sizeof(poruka));
	str_lcat(poruka, ")", sizeof(poruka));

	ui_entry_set_text(glavna_muzika_instanca->display, poruka);
}

void osvezi_gui_nakon_automatskog_prelaza(void)
{
	if (!glavna_muzika_instanca) return;

	char poruka[256];
	str_cpy(poruka, sizeof(poruka), "Slusate: ");
	str_lcat(poruka, trenutni_fajl, sizeof(poruka));
	ui_entry_set_text(glavna_muzika_instanca->display, poruka);

	for (int idx = 0; idx < ukupan_broj_pesama; idx++) {
		char stavka[160];
		if (idx == trenutni_indeks) {
			str_cpy(stavka, sizeof(stavka), "> ");
		} else {
			str_cpy(stavka, sizeof(stavka), "  ");
		}
		str_lcat(stavka, plejlista[idx], sizeof(stavka));
		ui_list_entry_set_caption(glavna_muzika_instanca->list_entries[idx], stavka);
	}
	ui_list_set_cursor(glavna_muzika_instanca->playlist_view, glavna_muzika_instanca->list_entries[trenutni_indeks]);
}

 void skeniraj_direktorijum(const char *putanja)
{
	DIR *dir = opendir(putanja);
	struct dirent *ent;
	ukupan_broj_pesama = 0;
	if (dir == NULL) return;

	while ((ent = readdir(dir)) != NULL && ukupan_broj_pesama < MAX_PESAMA) {
		if (str_cmp(ent->d_name, ".") == 0 || str_cmp(ent->d_name, "..") == 0)
			continue;
		const char *ext = str_rchr(ent->d_name, '.');
		if (ext != NULL) {
			if (str_casecmp(ext, ".mp3") == 0 || str_casecmp(ext, ".wav") == 0) {
				str_ncpy(plejlista[ukupan_broj_pesama], 128, ent->d_name, 127);
				ukupan_broj_pesama++;
			}
		}
	}
	closedir(dir);
}

static void playlist_activate_req(ui_list_t *list, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	if (ukupan_broj_pesama == 0) return;

	if (reprodukcija_u_toku) {
		ui_entry_set_text(muzika->display, "Zaustavite (STOP) pre biranja!");
		ui_list_set_cursor(muzika->playlist_view, muzika->list_entries[trenutni_indeks]);
		return;
	}

	ui_list_entry_t *cursor = ui_list_get_cursor(list);
	if (!cursor) return;

	for (int idx = 0; idx < ukupan_broj_pesama; idx++) {
		if (muzika->list_entries[idx] == cursor) {
			trenutni_indeks = idx;
			trenutni_fajl = plejlista[trenutni_indeks];
			char poruka[256];
			str_cpy(poruka, sizeof(poruka), "Selektovano: ");
			str_lcat(poruka, plejlista[trenutni_indeks], sizeof(poruka));
			ui_entry_set_text(muzika->display, poruka);
			break;
		}
	}
}

 ui_list_cb_t playlist_view_cb = { .activate_req = playlist_activate_req };

static void muzika_wnd_close(ui_window_t *window, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) window;
	ui_quit(muzika->ui);
}

static void muzika_wnd_kbd_event(ui_window_t *window, void *arg, kbd_event_t *event);

static ui_window_cb_t window_cb = {
	.close = muzika_wnd_close,
	.kbd = muzika_wnd_kbd_event // <--- SADA UKLJUČUJEMO CALLBACK ZA TASTATURU
};

static void muzika_wnd_kbd_event(ui_window_t *window, void *arg, kbd_event_t *event)
{
	muzika_t *muzika = (muzika_t *) arg;

	// Propuštamo podrazumevane UI događaje ako sistem želi da ih claims-uje
	if (ui_window_def_kbd(window, event) == ui_claimed)
		return;

	// Reagujemo samo na pritisak tastera (KEY_PRESS)
	if (event->type == KEY_PRESS) {
		switch (event->key) {
		case KC_UP: // Strelica na gore pojačava zvuk
			jacina_zvuka += 5;
			if (jacina_zvuka > 100) jacina_zvuka = 100; // Maksimalno 100%
			
			// Ispisujemo trenutnu jačinu na displeju na trenutak
			ui_entry_set_text(muzika->display, "Jacina zvuka pojacana");
			break;
			
		case KC_DOWN: // Strelica na dole utišava zvuk
			jacina_zvuka -= 5;
			if (jacina_zvuka < 0) jacina_zvuka = 0; // Minimalno 0% (Mute)
			
			ui_entry_set_text(muzika->display, "Jacina zvuka utisana");
			break;
			
		default:
			break;
		}
	}
}

 


static ui_pbutton_cb_t play_cb     = { .clicked = btn_play_clicked };
static ui_pbutton_cb_t pause_cb    = { .clicked = btn_pause_clicked };
static ui_pbutton_cb_t stop_cb     = { .clicked = btn_stop_clicked };
static ui_pbutton_cb_t playlist_cb = { .clicked = btn_playlist_clicked };

int main(int argc, char **argv)
{
	if (argc > 100) {
        printf("%s\n", srbin_verzija);
    }

	muzika_t muzika;
	ui_wnd_params_t wparams;
	ui_resource_t *ui_res;
	ui_list_entry_attr_t attr;
	gfx_rect_t btn_rect;
	errno_t rc;

	const char *display_spec = UI_ANY_DEFAULT; 
	int i = 1;

	while (i < argc) {
		if (str_cmp(argv[i], "-d") == 0) {
			++i;
			if (i >= argc) return EINVAL;
			display_spec = argv[i++];
		} else {
			return EINVAL;
		}
	}

	skeniraj_direktorijum(".");

	muzika.geom = muzika_geom_default;
	glavna_muzika_instanca = &muzika;

	rc = ui_create(display_spec, &muzika.ui);
	if (rc != EOK) return rc;

	ui_wnd_params_init(&wparams);
	wparams.caption = "SrBinOS Muzika Pro"; 
	wparams.rect.p0.x = 0;
	wparams.rect.p0.y = 0;
	wparams.rect.p1.x = 440; // Prošireno sa 320 na 440px
	wparams.rect.p1.y = 380; // Povećana visina za stabilan raspored i dugu listu


	rc = ui_window_create(muzika.ui, &wparams, &muzika.window);
	if (rc != EOK) return -1;
	
	ui_window_set_cb(muzika.window, &window_cb, &muzika);
	ui_res = ui_window_get_res(muzika.window);

	rc = ui_fixed_create(&muzika.fixed);
	if (rc != EOK) return -1;

// =================================================================
	// KREIRANJE GLAVNOG MENIJA NA VRHU PROZORA (PREBAČENO U MENI.C)
	// =================================================================
	rc = kreiraj_meni(&muzika);
	if (rc != EOK) {
		printf("[UPOZORENJE] Neuspesno kreiranje menija.\n");
	}


	// Stabilan ekran
	const char *pocetni_tekst = (ukupan_broj_pesama > 0) ? plejlista[0] : "Nema pronadjenih pesama";
	rc = ui_entry_create(muzika.window, pocetni_tekst, &muzika.display);
	if (rc != EOK) return -1;
	ui_entry_set_read_only(muzika.display, true);
	ui_entry_set_rect(muzika.display, &muzika.geom.display_rect);
	ui_fixed_add(muzika.fixed, ui_entry_ctl(muzika.display));

	if (ukupan_broj_pesama > 0) {
		trenutni_fajl = plejlista[0];
	}

	// Stabilna lista
	rc = ui_list_create(muzika.window, false, &muzika.playlist_view);
	if (rc == EOK) {
		ui_list_entry_attr_init(&attr);
		for (int idx = 0; idx < ukupan_broj_pesama; idx++) {
			char stavka[160];
			if (idx == 0) {
				str_cpy(stavka, sizeof(stavka), "> ");
			} else {
				str_cpy(stavka, sizeof(stavka), "  ");
			}
			str_lcat(stavka, plejlista[idx], sizeof(stavka));
			attr.caption = stavka;
			ui_list_entry_append(muzika.playlist_view, &attr, &muzika.list_entries[idx]);
		}
		ui_list_set_rect(muzika.playlist_view, &muzika.geom.list_rect);
		if (ukupan_broj_pesama > 0) {
			ui_list_set_cursor(muzika.playlist_view, muzika.list_entries[0]);
		}
		ui_list_set_cb(muzika.playlist_view, &playlist_view_cb, &muzika);
		ui_fixed_add(muzika.fixed, ui_list_ctl(muzika.playlist_view));
	}

		// =================================================================
	// KREIRANJE GORNJEG REDA DUGMADI (PREV, PLAY, PAUZA, STOP, NEXT, LIST)
	// =================================================================
	btn_rect.p0 = muzika.geom.btn_orig;
	btn_rect.p1.y = btn_rect.p0.y + muzika.geom.btn_dim.y;

	// 1. DUGME: PREV (Prethodna pesma)
	rc = ui_pbutton_create(ui_res, "PREV", &muzika.btn_prev);
	ui_pbutton_set_cb(muzika.btn_prev, &prev_cb, &muzika);
	btn_rect.p1.x = btn_rect.p0.x + muzika.geom.btn_dim.x;
	ui_pbutton_set_rect(muzika.btn_prev, &btn_rect);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_prev));

	// 2. DUGME: PLAY (Sviraj / Nastavi)
	rc = ui_pbutton_create(ui_res, "PLAY", &muzika.btn_play);
	ui_pbutton_set_cb(muzika.btn_play, &play_cb, &muzika);
	btn_rect.p0.x += muzika.geom.btn_stride.x;
	btn_rect.p1.x = btn_rect.p0.x + muzika.geom.btn_dim.x;
	ui_pbutton_set_rect(muzika.btn_play, &btn_rect);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_play));

	// 3. DUGME: PAUZA (Pauziraj)
	rc = ui_pbutton_create(ui_res, "PAUZA", &muzika.btn_pause);
	ui_pbutton_set_cb(muzika.btn_pause, &pause_cb, &muzika);
	btn_rect.p0.x += muzika.geom.btn_stride.x;
	btn_rect.p1.x = btn_rect.p0.x + muzika.geom.btn_dim.x;
	ui_pbutton_set_rect(muzika.btn_pause, &btn_rect);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_pause));

	// 4. DUGME: STOP (Zaustavi)
	rc = ui_pbutton_create(ui_res, "STOP", &muzika.btn_stop);
	ui_pbutton_set_cb(muzika.btn_stop, &stop_cb, &muzika);
	btn_rect.p0.x += muzika.geom.btn_stride.x;
	btn_rect.p1.x = btn_rect.p0.x + muzika.geom.btn_dim.x;
	ui_pbutton_set_rect(muzika.btn_stop, &btn_rect);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_stop));

	// 5. DUGME: NEXT (Sledeća pesma)
	rc = ui_pbutton_create(ui_res, "NEXT", &muzika.btn_next);
	ui_pbutton_set_cb(muzika.btn_next, &next_cb, &muzika);
	btn_rect.p0.x += muzika.geom.btn_stride.x;
	btn_rect.p1.x = btn_rect.p0.x + muzika.geom.btn_dim.x;
	ui_pbutton_set_rect(muzika.btn_next, &btn_rect);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_next));

	// 6. DUGME: LIST (Smanjeno i gurnuto skroz desno na kraj reda)
	rc = ui_pbutton_create(ui_res, "LIST", &muzika.btn_playlist);
	ui_pbutton_set_cb(muzika.btn_playlist, &playlist_cb, &muzika);
	
	gfx_rect_t list_btn_rect;
	list_btn_rect.p0.x = 370; // Ručno gurnuto na desnu ivicu da ostane prazan prostor pre njega
	list_btn_rect.p1.x = 425; // Širina 55px, poravnato sa uspravnim dugmićima ispod
	list_btn_rect.p0.y = muzika.geom.btn_orig.y;
	list_btn_rect.p1.y = list_btn_rect.p0.y + muzika.geom.btn_dim.y;
	
	ui_pbutton_set_rect(muzika.btn_playlist, &list_btn_rect);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_playlist));


	// =================================================================
	// KREIRANJE DESNIH USPRANIH DUGMADI ZA PLEJLISTU SA CALLBACK-OVIMA
	// =================================================================
	gfx_rect_t v_btn_rect;
	v_btn_rect.p0.x = 335; 
	v_btn_rect.p1.x = 425; 
	
	// 1. Uspravno dugme: UCITAJ (Učitaj ceo folder)
	v_btn_rect.p0.y = 145; 
	v_btn_rect.p1.y = v_btn_rect.p0.y + 28;
	rc = ui_pbutton_create(ui_res, "UCITAJ", &muzika.btn_l_ucitaj);
	ui_pbutton_set_rect(muzika.btn_l_ucitaj, &v_btn_rect);
	// POVEZIVANJE CALLBACK-A:
	ui_pbutton_set_cb(muzika.btn_l_ucitaj, &l_ucitaj_cb, &muzika);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_l_ucitaj));

	// 2. Uspravno dugme: PESMA (Učitaj pojedinačnu pesmu)
	v_btn_rect.p0.y += 36; 
	v_btn_rect.p1.y = v_btn_rect.p0.y + 28;
	rc = ui_pbutton_create(ui_res, "PESMA", &muzika.btn_l_pesma);
	ui_pbutton_set_rect(muzika.btn_l_pesma, &v_btn_rect);
	// POVEZIVANJE CALLBACK-A:
	ui_pbutton_set_cb(muzika.btn_l_pesma, &l_pesma_cb, &muzika);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_l_pesma));

	// 3. Uspravno dugme: OSVEZI (Ponovo skeniraj direktorijum)
	v_btn_rect.p0.y += 36;
	v_btn_rect.p1.y = v_btn_rect.p0.y + 28;
	rc = ui_pbutton_create(ui_res, "OSVEZI", &muzika.btn_l_osvezi);
	ui_pbutton_set_rect(muzika.btn_l_osvezi, &v_btn_rect);
	// POVEZIVANJE CALLBACK-A:
	ui_pbutton_set_cb(muzika.btn_l_osvezi, &l_osvezi_cb, &muzika);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_l_osvezi));

	// 4. Uspravno dugme: OBRISI (Ukloni pesmu iz plejliste)
	v_btn_rect.p0.y += 36;
	v_btn_rect.p1.y = v_btn_rect.p0.y + 28;
	rc = ui_pbutton_create(ui_res, "OBRISI", &muzika.btn_l_obrisi);
	ui_pbutton_set_rect(muzika.btn_l_obrisi, &v_btn_rect);
	// POVEZIVANJE CALLBACK-A:
	ui_pbutton_set_cb(muzika.btn_l_obrisi, &l_obrisi_cb, &muzika);
	ui_fixed_add(muzika.fixed, ui_pbutton_ctl(muzika.btn_l_obrisi));
	



        ui_window_add(muzika.window, ui_fixed_ctl(muzika.fixed));
        rc = ui_window_paint(muzika.window);
        if (rc != EOK) return -1;
        
        ui_run(muzika.ui);
        return 0;
        }
        