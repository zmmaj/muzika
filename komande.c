#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fibril.h>
#include <str.h>
#include <ui/ui.h>
#include <ui/window.h>
#include <ui/filedialog.h>
#include "komande.h"
#include "audio.h"

extern char plejlista[MAX_PESAMA][128];
extern int ukupan_broj_pesama;
extern int trenutni_indeks;
extern const char *trenutni_fajl;

extern void skeniraj_direktorijum(const char *putanja);

// Pomoćna funkcija koja briše i ponovo iscrtava listu od nule (Recreate pristup)
static void ponovo_izgradi_graficku_listu(muzika_t *muzika)
{
	if (muzika->playlist_view != NULL) {
		ui_list_destroy(muzika->playlist_view);
		muzika->playlist_view = NULL;
	}

	for (int i = 0; i < MAX_PESAMA; i++) {
		muzika->list_entries[i] = NULL;
	}

	errno_t rc = ui_list_create(muzika->window, false, &muzika->playlist_view);
	if (rc == EOK) {
		ui_list_entry_attr_t attr;
		ui_list_entry_attr_init(&attr);
		
		for (int idx = 0; idx < ukupan_broj_pesama; idx++) {
			attr.caption = plejlista[idx];
			ui_list_entry_append(muzika->playlist_view, &attr, &muzika->list_entries[idx]);
		}
		
		ui_list_set_rect(muzika->playlist_view, &muzika->geom.list_rect);
		extern ui_list_cb_t playlist_view_cb;
		ui_list_set_cb(muzika->playlist_view, &playlist_view_cb, muzika);
		
		ui_fixed_add(muzika->fixed, ui_list_ctl(muzika->playlist_view));
		ui_fixed_paint(muzika->fixed);
		ui_window_paint(muzika->window);
	}
}

// --- CALLBACK STRUKTURE PREMA VEĆ VIĐENOM PRIMERU IZ UIDEMO.C ---
static ui_file_dialog_cb_t muzika_fd_cb = {
	.bok = muzika_file_dialog_bok,
	.bcancel = muzika_file_dialog_bcancel,
	.close = muzika_file_dialog_close
};

static ui_prompt_dialog_cb_t muzika_pd_cb = {
	.bok = muzika_prompt_dialog_bok,
	.bcancel = muzika_prompt_dialog_bcancel,
	.close = muzika_prompt_dialog_close
};

// =================================================================
// REŽIM 1: LOGIKA ZA DUGME "PESMA" (GRAFIČKI FILE DIALOG)
// =================================================================

void btn_l_pesma_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;
	
	ui_file_dialog_params_t fdparams;
	ui_file_dialog_t *dialog = NULL;
	ui_file_dialog_params_init(&fdparams);
	fdparams.caption = "Izaberi audio fajl (.mp3, .wav)";

	if (ui_file_dialog_create(muzika->ui, &fdparams, &dialog) == EOK) {
		ui_file_dialog_set_cb(dialog, &muzika_fd_cb, muzika);
	}
}

void muzika_file_dialog_bok(ui_file_dialog_t *dialog, void *arg, const char *fname)
{
	muzika_t *muzika = (muzika_t *) arg;
	ui_file_dialog_destroy(dialog);
	
	if (!fname || str_cmp(fname, "") == 0) return;

	if (ukupan_broj_pesama < MAX_PESAMA) {
		// Nativno dodavanje samo jedne izabrane pesme na kraj liste
		str_ncpy(plejlista[ukupan_broj_pesama], 128, fname, 127);
		
		ui_list_entry_attr_t attr;
		ui_list_entry_attr_init(&attr);
		attr.caption = plejlista[ukupan_broj_pesama];
		ui_list_entry_append(muzika->playlist_view, &attr, &muzika->list_entries[ukupan_broj_pesama]);
		
		trenutni_indeks = ukupan_broj_pesama;
		trenutni_fajl = plejlista[trenutni_indeks];
		ukupan_broj_pesama++;

		char poruka[256];
		str_cpy(poruka, sizeof(poruka), "Dodata pesma: ");
		str_lcat(poruka, trenutni_fajl, sizeof(poruka));
		ui_entry_set_text(muzika->display, poruka);

		ui_list_set_cursor(muzika->playlist_view, muzika->list_entries[trenutni_indeks]);
	}
}

void muzika_file_dialog_bcancel(ui_file_dialog_t *dialog, void *arg) { (void)arg; ui_file_dialog_destroy(dialog); }
void muzika_file_dialog_close(ui_file_dialog_t *dialog, void *arg) { (void)arg; ui_file_dialog_destroy(dialog); }

// =================================================================
// REŽIM 2: LOGIKA ZA DUGME "UČITAJ" (PROMPT DIALOG ZA DIREKTORIJUM)
// =================================================================

void btn_l_ucitaj_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;
	
	ui_prompt_dialog_params_t pdparams;
	ui_prompt_dialog_t *dialog = NULL;
	ui_prompt_dialog_params_init(&pdparams);
	pdparams.caption = "Ucitaj kompletan folder";
	pdparams.prompt = "Unesite putanju direktorijuma (npr. . ili /data):";

	if (ui_prompt_dialog_create(muzika->ui, &pdparams, &dialog) == EOK) {
		ui_prompt_dialog_set_cb(dialog, &muzika_pd_cb, muzika);
	}
}

void muzika_prompt_dialog_bok(ui_prompt_dialog_t *dialog, void *arg, const char *text)
{
	muzika_t *muzika = (muzika_t *) arg;
	ui_prompt_dialog_destroy(dialog);
	
	if (!text || str_cmp(text, "") == 0) return;

	ui_entry_set_text(muzika->display, "Skeniram folder i podfoldere...");

	// Pokrećemo rekurzivni automatski skener nad upisanim direktorijumom
	ukupan_broj_pesama = 0;
	trenutni_indeks = 0;
	skeniraj_direktorijum(text);

	// Ponovo gradimo grafičku listu da prikažemo rekurzivne rezultate
	ponovo_izgradi_graficku_listu(muzika);

	if (ukupan_broj_pesama > 0) {
		trenutni_fajl = plejlista[0];
		ui_list_set_cursor(muzika->playlist_view, muzika->list_entries[0]);
		ui_entry_set_text(muzika->display, "Direktorijum uspesno ucitan.");
	} else {
		trenutni_fajl = NULL;
		ui_entry_set_text(muzika->display, "Nema podrzanih pesama na toj putanji.");
	}
}

void muzika_prompt_dialog_bcancel(ui_prompt_dialog_t *dialog, void *arg) { (void)arg; ui_prompt_dialog_destroy(dialog); }
void muzika_prompt_dialog_close(ui_prompt_dialog_t *dialog, void *arg) { (void)arg; ui_prompt_dialog_destroy(dialog); }

// =================================================================
// PREOSTALE OSNOVNE KOMANDE
// =================================================================

void btn_next_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;
	if (ukupan_broj_pesama == 0) return;
	
	bool bila_aktivna = reprodukcija_u_toku;
	
	// 1. KORAK: MOMENTALNO ZAUSTAVLJAMO TRENUTNU PESMU AKO SVIRA
	if (bila_aktivna) {
		extern void btn_stop_clicked(ui_pbutton_t *button, void *arg);
		btn_stop_clicked(NULL, muzika); // Pokreće pražnjenje i postavlja reprodukcija_u_toku = false
		
		// --- KLJUČNA POPRAVKA: Čekamo 2 sekunde da drajver potpuno iskrvari zaostali bafer ---
		// 2.000.000 mikrosekundi = 2 sekunde čistog mira za zvučni čip
		fibril_usleep(2000000); 
	}
	
	// 2. KORAK: TEK KADA SE BAFER ISPRAZNIO I SVE UTIHNULO, MENJAMO INDEKS
	if (ukupan_broj_pesama == 1) {
		trenutni_indeks = 0;
	} else if (trenutni_indeks == ukupan_broj_pesama - 1) {
		trenutni_indeks = 0;
	} else {
		trenutni_indeks++;
	}
	
	trenutni_fajl = plejlista[trenutni_indeks];
	osvezi_gui_nakon_automatskog_prelaza();
	
	// 3. KORAK: PUSTAMO SLEDEĆU PESMU SA POTPUNO ČISTIM BAFEROM
	if (bila_aktivna) {
		btn_play_clicked(NULL, muzika);
	}
}

void btn_prev_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;
	if (ukupan_broj_pesama == 0) return;
	
	bool bila_aktivna = reprodukcija_u_toku;
	
	// 1. KORAK: MOMENTALNO ZAUSTAVLJAMO ZVUK
	if (bila_aktivna) {
		extern void btn_stop_clicked(ui_pbutton_t *button, void *arg);
		btn_stop_clicked(NULL, muzika);
		
		// Čekamo 2 sekunde da se isprazni zaostali audio bafer
		fibril_usleep(2000000); 
	}
	
	// 2. KORAK: PRORAČUN INDEKSA UNAZAD
	if (ukupan_broj_pesama == 1) {
		trenutni_indeks = 0;
	} else if (trenutni_indeks == 0) {
		trenutni_indeks = ukupan_broj_pesama - 1;
	} else {
		trenutni_indeks--;
	}
	
	trenutni_fajl = plejlista[trenutni_indeks];
	osvezi_gui_nakon_automatskog_prelaza();
	
	// 3. KORAK: PALIMO REPRODUKCIJU
	if (bila_aktivna) {
		btn_play_clicked(NULL, muzika);
	}
}



void btn_l_osvezi_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;
	ui_entry_set_text(muzika->display, "Automatsko osvezavanje...");
	ukupan_broj_pesama = 0;
	trenutni_indeks = 0;
	skeniraj_direktorijum(".");
	ponovo_izgradi_graficku_listu(muzika);
}

void btn_l_obrisi_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;
	reprodukcija_u_toku = false;
	reprodukcija_pauzirana = false;
	ukupan_broj_pesama = 0;
	trenutni_indeks = 0;
	trenutni_fajl = NULL;
	ponovo_izgradi_graficku_listu(muzika);
	ui_entry_set_text(muzika->display, "Plejlista je ispraznjena.");
}

void btn_play_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;

	if (ukupan_broj_pesama == 0) return;

	if (reprodukcija_u_toku && reprodukcija_pauzirana) {
		reprodukcija_pauzirana = false;
		ui_entry_set_text(muzika->display, "Nastavljeno...");
		return;
	}

	if (reprodukcija_u_toku) return;

	reprodukcija_u_toku = true;
	reprodukcija_pauzirana = false;

	char poruka[256];
	str_cpy(poruka, sizeof(poruka), "Slusate: ");
	str_lcat(poruka, trenutni_fajl, sizeof(poruka));
	ui_entry_set_text(muzika->display, poruka);

	fid_t fid = fibril_create(audio_thread_func, NULL);
	if (fid != 0) fibril_start(fid);
}

 void btn_pause_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;
	if (reprodukcija_u_toku && !reprodukcija_pauzirana) {
		reprodukcija_pauzirana = true;
		ui_entry_set_text(muzika->display, "Status: PAUZIRANO");
	}
}

 void btn_stop_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;
	if (reprodukcija_u_toku) {
		reprodukcija_u_toku = false;
		ui_entry_set_text(muzika->display, "Zaustavljeno.");
	}
}

void btn_playlist_clicked(ui_pbutton_t *button, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) button;

	if (ukupan_broj_pesama == 0) return;

	if (reprodukcija_u_toku) return;

	trenutni_indeks = (trenutni_indeks + 1) % ukupan_broj_pesama;
	trenutni_fajl = plejlista[trenutni_indeks];

	char poruka[256];
	str_cpy(poruka, sizeof(poruka), "Selektovano: ");
	str_lcat(poruka, trenutni_fajl, sizeof(poruka));
	ui_entry_set_text(muzika->display, poruka);

	for (int idx = 0; idx < ukupan_broj_pesama; idx++) {
		char stavka[160];
		if (idx == trenutni_indeks) str_cpy(stavka, sizeof(stavka), "> ");
		else str_cpy(stavka, sizeof(stavka), "  ");
		str_lcat(stavka, plejlista[idx], sizeof(stavka));
		ui_list_entry_set_caption(muzika->list_entries[idx], stavka);
	}
	ui_list_set_cursor(muzika->playlist_view, muzika->list_entries[trenutni_indeks]);
}

ui_pbutton_cb_t prev_cb     = { .clicked = btn_prev_clicked };
ui_pbutton_cb_t next_cb     = { .clicked = btn_next_clicked };
ui_pbutton_cb_t l_ucitaj_cb = { .clicked = btn_l_ucitaj_clicked };
ui_pbutton_cb_t l_pesma_cb  = { .clicked = btn_l_pesma_clicked };
ui_pbutton_cb_t l_osvezi_cb = { .clicked = btn_l_osvezi_clicked };
ui_pbutton_cb_t l_obrisi_cb = { .clicked = btn_l_obrisi_clicked };