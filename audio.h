#ifndef AUDIO_H_
#define AUDIO_H_

#include <stdbool.h>
#include <errno.h>

#define MAX_PESAMA 64

/* Funkcija niti */
errno_t audio_thread_func(void *arg);

/* Funkcija za osvežavanje GUI interfejsa */
void osvezi_gui_nakon_automatskog_prelaza(void);
void azurirajte_vreme_u_gui(void);
unsigned char* create_wav_from_mp3(const char* filename);
void sviraj(unsigned char* source, size_t size);


/* Eksterni pokazivači koje dele main.c i audio.c */
extern const char *trenutni_fajl;
extern bool reprodukcija_u_toku;
extern bool reprodukcija_pauzirana;

// Tačan dvodimenzionalni tip deljene plejliste
extern char plejlista[MAX_PESAMA][128];
extern int ukupan_broj_pesama;
extern int trenutni_indeks;

// Deljene promenljive za vreme
extern int ukupno_sekundi;
extern int trenutna_sekunda;

extern int jacina_zvuka;

#endif /* AUDIO_H_ */
