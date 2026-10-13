#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <task.h>  
#include <ui/menubar.h>
#include <ui/menu.h>
#include <ui/menudd.h>
#include <ui/menuentry.h>
#include "meni.h"
#include "audio.h"

#define HELP_APP "/app/test_html"
// --- AKCIJE ZA KLIK NA STAVKE MENIJA ---

static void menu_exit_activate(ui_menu_entry_t *me, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) me;
	
	reprodukcija_u_toku = false;
	ui_quit(muzika->ui);
}

/** Pomoc / O Programu (Procitaj me) stavka iz menija izabrana.
 * Pokreće sistemski HTML pregledač i učitava uputstvo za plejer.
 */
static void menu_about_activate(ui_menu_entry_t *me, void *arg)
{
	muzika_t *muzika = (muzika_t *) arg;
	(void) me;
	
	// Putanja do novog HTML uputstva koje smo upravo ažurirali
	const char *help_file = "data/pomoc/muzika.html";
	task_id_t tid;
	errno_t rc;

	// Obaveštavamo korisnika na ekranu pre paljenja eksterne aplikacije
	ui_entry_set_text(muzika->display, "Otvaram pomoc korisniku...");

	// Pokrećemo test_html i prosleđujemo putanju do fajla pomoći
	rc = task_spawnl(&tid, NULL, HELP_APP, HELP_APP, help_file, NULL);
	if (rc != EOK) {
		printf("[GRESKA] Neuspelo pokretanje pomoci u (%s)\n", HELP_APP);
		ui_entry_set_text(muzika->display, "Greska: Pomoc nedostupna!");
	}
}


// --- GLAVNA FUNKCIJA ZA KREIRANJE MENIJA ---

errno_t kreiraj_meni(muzika_t *muzika)
{
	if (!muzika || !muzika->ui || !muzika->window) {
		return EINVAL;
	}

	errno_t rc;

	// 1. Kreiranje same trake menija na vrhu prozora
	rc = ui_menu_bar_create(muzika->ui, muzika->window, &muzika->menu_bar);
	if (rc != EOK) {
		return rc;
	}

	// Dodavanje menija u fixed raspored prozora radi iscrtavanja i događaja
	ui_fixed_add(muzika->fixed, ui_menu_bar_ctl(muzika->menu_bar));

	// Pozicioniranje trake menija na sam vrh (visina 20px je stabilna za HelenOS menije)
	gfx_rect_t mbar_rect = { {0, 20}, {440, 40} };
	ui_menu_bar_set_rect(muzika->menu_bar, &mbar_rect);

	// 2. Kreiranje "File" padajućeg menija i stavke "Izlaz"
	rc = ui_menu_dd_create(muzika->menu_bar, "File", NULL, &muzika->menu_file);
	if (rc == EOK) {
		rc = ui_menu_entry_create(muzika->menu_file, "Izlaz", "Ctrl+Q", &muzika->me_exit);
		if (rc == EOK) {
			// Prosljeđuje se direktno funkcija handler, baš kao u Slikovnici!
			ui_menu_entry_set_cb(muzika->me_exit, menu_exit_activate, muzika);
		}
	}

	// 3. Kreiranje "Help" padajućeg menija i stavke "O Programu"
	rc = ui_menu_dd_create(muzika->menu_bar, "Help", NULL, &muzika->menu_help);
	if (rc == EOK) {
		rc = ui_menu_entry_create(muzika->menu_help, "O Programu", "F1", &muzika->me_about);
		if (rc == EOK) {
			ui_menu_entry_set_cb(muzika->me_about, menu_about_activate, muzika);
		}
	}

	return EOK;
}
