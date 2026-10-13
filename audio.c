#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fibril.h> // Neophodno za fibril_yield() unutar petlje

// Tvoj originalni dr_mp3 dekoder koji radi bez greške
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

// SrBinOS i Hound audio zaglavlja
#include "wave.h"
#include <hound/client.h>
#include <pcm/sample_format.h>
#include <str_error.h>
#include <str.h>
#include <mem.h>
#include "audio.h"


#define MAX_PESAMA 64
extern char plejlista[MAX_PESAMA][128];
extern int ukupan_broj_pesama;
extern int trenutni_indeks;

int ukupno_sekundi = 0;
int trenutna_sekunda = 0;

int jacina_zvuka = 80;

#define READ_SIZE            (32 * 1024)
#define STREAM_BUFFER_SIZE   (64 * 1024)
#define PCM_BUFFER_SIZE      (64 * 1024)

/* Globalne kontrolne zastavice za main.c GUI */
const char *trenutni_fajl = "acdc.mp3";
bool reprodukcija_u_toku = false;
bool reprodukcija_pauzirana = false;

// --- TVOJ ORIGINALNI KOD ZA DEKODIRANJE ---
unsigned char* create_wav_from_mp3(const char* filename) {
    if (!filename) {
        printf("[GRESKA] Putanja do MP3 fajla nije validna.\n");
        return NULL;
    }

    drmp3 mp3;
    if (!drmp3_init_file(&mp3, filename, NULL)) {
        printf("[GRESKA] Neuspesna inicijalizacija MP3 fajla.\n");
        return NULL;
    }

    if (mp3.channels == 0 || mp3.sampleRate == 0) {
        drmp3_uninit(&mp3);
        return NULL;
    }

    float* pcmData = (float*)malloc(PCM_BUFFER_SIZE);
    if (!pcmData) {
        drmp3_uninit(&mp3);
        return NULL;
    }

    size_t totalFrames = 0;
    size_t maxFrames = PCM_BUFFER_SIZE / (mp3.channels * sizeof(float));
    uint16_t* pcmData16 = NULL;

    while (1) {
        size_t framesDecoded = drmp3_read_pcm_frames_f32(&mp3, maxFrames, pcmData);
        if (framesDecoded == 0) break;

        uint16_t* temp = (uint16_t*)realloc(pcmData16, (totalFrames + framesDecoded) * mp3.channels * sizeof(uint16_t));
        if (!temp) {
            free(pcmData);
            free(pcmData16);
            drmp3_uninit(&mp3);
            return NULL;
        }
        pcmData16 = temp;

        for (size_t i = 0; i < framesDecoded * mp3.channels; i++) {
            float sample = pcmData[i] * 32767.0f;
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            pcmData16[totalFrames * mp3.channels + i] = (uint16_t)((int16_t)sample);
        }

        totalFrames += framesDecoded;
    }

    free(pcmData);
    drmp3_uninit(&mp3);

    unsigned char wavHeader[44] = {
        'R', 'I', 'F', 'F',
        0x00, 0x00, 0x00, 0x00,
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        0x10, 0x00, 0x00, 0x00,
        0x01, 0x00,
        0x02, 0x00,
        0x44, 0xAC, 0x00, 0x00,
        0x10, 0xB1, 0x02, 0x00,
        0x04, 0x00,
        0x10, 0x00,
        'd', 'a', 't', 'a',
        0x00, 0x00, 0x00, 0x00
    };

    size_t dataSize = totalFrames * mp3.channels * sizeof(uint16_t);
    size_t fileSize = dataSize + 36;

    wavHeader[4] = (unsigned char)(fileSize & 0xFF);
    wavHeader[5] = (unsigned char)((fileSize >> 8) & 0xFF);
    wavHeader[6] = (unsigned char)((fileSize >> 16) & 0xFF);
    wavHeader[7] = (unsigned char)((fileSize >> 24) & 0xFF);

    wavHeader[40] = (unsigned char)(dataSize & 0xFF);
    wavHeader[41] = (unsigned char)((dataSize >> 8) & 0xFF);
    wavHeader[42] = (unsigned char)((dataSize >> 16) & 0xFF);
    wavHeader[43] = (unsigned char)((dataSize >> 24) & 0xFF);

    unsigned char* wavData = (unsigned char*)malloc(44 + dataSize);
    if (!wavData) {
        free(pcmData16);
        return NULL;
    }

    memcpy(wavData, wavHeader, 44);
    memcpy(wavData + 44, pcmData16, dataSize);
    free(pcmData16);

    return wavData;
}

// --- OPTIMIZOVAN KOD ZA SVIRANJE (UKLONJEN UNDERRUN) ---
void sviraj(unsigned char* source, size_t size) {
    wave_header_t header;
    memcpy(&header, source, sizeof(header));

    pcm_format_t format;
    const char *GRESKA;
    errno_t ret = wav_parse_header(&header, NULL, NULL, &format.channels,
                                   &format.sampling_rate, &format.sample_format, &GRESKA);
    if (ret != EOK) {
        return;
    }

    hound_context_t *ctx = hound_context_create_playback("wavplay", format, STREAM_BUFFER_SIZE);
    if (!ctx) return;

    ret = hound_context_connect_target(ctx, HOUND_DEFAULT_TARGET);
    if (ret != EOK) {
        hound_context_destroy(ctx);
        return;
    }

    hound_stream_t *stream = hound_stream_create(ctx, HOUND_STREAM_DRAIN_ON_EXIT, format, STREAM_BUFFER_SIZE);
    if (!stream) {
        hound_context_destroy(ctx);
        return;
    }

    size_t offset = sizeof(header);
    size_t audio_data_only_size = size - offset;
    
    char *tisina_buffer = calloc(1, READ_SIZE); 
    
    // Alociramo privremeni bafer u koji ćemo kopirati i skalirati jačinu trenutnog bloka
    char *volume_buffer = malloc(READ_SIZE);

    while (offset < size && reprodukcija_u_toku) {
        if (reprodukcija_pauzirana) {
            if (tisina_buffer) {
                ret = hound_stream_write(stream, tisina_buffer, READ_SIZE);
                if (ret != EOK) break;
            }
            fibril_yield();
            continue;
        }

        size_t bytes_to_read = (size - offset < READ_SIZE) ? (size - offset) : READ_SIZE;

        // --- SOFTVERSKO SKALIRANJE JAČINE ZVUKA (VOLUME) ---
        if (volume_buffer) {
            // Izračunavamo faktor množenja (jacina_zvuka je od 0 do 100, faktor je od 0.0 do 1.0)
            float faktor = (float)jacina_zvuka / 100.0f;
            
            // Kastujemo pokazivače u 16-bitne uzorke (PCM SINT16_LE)
            int16_t *izvor_uzorci = (int16_t *)(source + offset);
            int16_t *odrediste_uzorci = (int16_t *)volume_buffer;
            size_t broj_uzoraka = bytes_to_read / sizeof(int16_t);

            for (size_t i = 0; i < broj_uzoraka; i++) {
                float skalirani_uzorak = (float)izvor_uzorci[i] * faktor;
                
                // Bezbjedni clipping (da sprečimo distorziju zvuka)
                if (skalirani_uzorak > 32767.0f) skalirani_uzorak = 32767.0f;
                if (skalirani_uzorak < -32768.0f) skalirani_uzorak = -32768.0f;
                
                odrediste_uzorci[i] = (int16_t)skalirani_uzorak;
            }

            // Šaljemo modifikovani bafer sa prilagođenom jačinom zvuka
            ret = hound_stream_write(stream, volume_buffer, bytes_to_read);
        } else {
            // Fallback ako alokacija bafera ne uspe (svira nativnu jačinu)
            ret = hound_stream_write(stream, source + offset, bytes_to_read);
        }

        if (ret != EOK) break;

        offset += bytes_to_read;

        // --- DINAMIČKO RAČUNANJE I OSVEŽAVANJE DIGITALNOG VREMENA ---
        size_t predjeno_bajtova = offset - sizeof(header);
        if (audio_data_only_size > 0 && ukupno_sekundi > 0) {
            trenutna_sekunda = (int)(((double)predjeno_bajtova / audio_data_only_size) * ukupno_sekundi);
            if (trenutna_sekunda > ukupno_sekundi) {
                trenutna_sekunda = ukupno_sekundi;
            }
            azurirajte_vreme_u_gui();
        }

        fibril_yield();
    }

    if (tisina_buffer) free(tisina_buffer);
    if (volume_buffer) free(volume_buffer);
    
    hound_stream_destroy(stream);
    hound_context_destroy(ctx);
}



errno_t audio_thread_func(void *arg)
{
	(void) arg;
	
	while (reprodukcija_u_toku) {
		drmp3 mp3;
		const char *ext = str_rchr(trenutni_fajl, '.');

		// =================================================================
		// REŽIM 1: AKO JE FAJL ČIST WAV (Preskače dr_mp3 i svira direktno)
		// =================================================================
		if (ext != NULL && str_casecmp(ext, ".wav") == 0) {
			FILE *f = fopen(trenutni_fajl, "rb");
			if (f) {
				fseek(f, 0, SEEK_END);
				long fsize = ftell(f);
				fseek(f, 0, SEEK_SET);

				unsigned char *wavData = malloc(fsize);
				if (wavData) {
					fread(wavData, 1, fsize, f);
					fclose(f);

					// RAČUNANJE TRAJANJA ZA WAV: Parsiramo zaglavlje da dobijemo Hz i kanale
					wave_header_t w_header;
					memcpy(&w_header, wavData, sizeof(w_header)); // Privremeno ime da ne brka kompajler
					
					unsigned w_channels = 0, w_rate = 0;
					pcm_sample_format_t w_fmt;
					const char *w_err;
					
					if (wav_parse_header(wavData, NULL, NULL, &w_channels, &w_rate, &w_fmt, &w_err) == 0) {
						size_t data_bytes = fsize - 44;
						// s16 format ima 2 bajta po uzorku
						uint64_t total_samples = data_bytes / 2;
						uint64_t total_frames = total_samples / w_channels;
						
						ukupno_sekundi = (int)(total_frames / w_rate);
					} else {
						ukupno_sekundi = 0;
					}
					trenutna_sekunda = 0;

					// Puštamo ispravan WAV direktno u Hound drajver
					sviraj(wavData, fsize);
					free(wavData);
				} else {
					fclose(f);
				}
			} else {
				printf("[HOUND-Muzika] Neuspesno otvaranje WAV fajla: %s. Preskacem...\n", trenutni_fajl);
				if (ukupan_broj_pesama > 0) {
					trenutni_indeks = (trenutni_indeks + 1) % ukupan_broj_pesama;
					trenutni_fajl = plejlista[trenutni_indeks];
					osvezi_gui_nakon_automatskog_prelaza();
				} else {
					reprodukcija_u_toku = false;
					break;
				}
				continue;
			}

			// Kada se WAV završi, automatski šaltamo na sledeću
			if (reprodukcija_u_toku) {
				trenutni_indeks = (trenutni_indeks + 1) % ukupan_broj_pesama;
				trenutni_fajl = plejlista[trenutni_indeks];
				osvezi_gui_nakon_automatskog_prelaza();
			}
			continue;
		}

		// =================================================================
		// REŽIM 2: AKO JE FAJL MP3 (Dekodiranje i računanje dužine)
		// =================================================================
		if (!drmp3_init_file(&mp3, trenutni_fajl, NULL)) {
			printf("[HOUND-Muzika] %s je ostecen ili nije MP3/WAV. Preskacem...\n", trenutni_fajl);
			
			if (ukupan_broj_pesama > 0) {
				trenutni_indeks = (trenutni_indeks + 1) % ukupan_broj_pesama;
				trenutni_fajl = plejlista[trenutni_indeks];
				osvezi_gui_nakon_automatskog_prelaza();
			} else {
				reprodukcija_u_toku = false;
				break;
			}
			continue; 
		}

		if (mp3.channels == 0 || mp3.sampleRate == 0) {
			drmp3_uninit(&mp3);
			if (ukupan_broj_pesama > 0) {
				trenutni_indeks = (trenutni_indeks + 1) % ukupan_broj_pesama;
				trenutni_fajl = plejlista[trenutni_indeks];
				osvezi_gui_nakon_automatskog_prelaza();
			}
			continue;
		}

		// RAČUNANJE TRAJANJA ZA MP3: Matematika preko ukupnog broja frejmova i brzine uzorkovanja
		uint64_t total_mp3_frames = drmp3_get_pcm_frame_count(&mp3);
		ukupno_sekundi = (int)(total_mp3_frames / mp3.sampleRate);
		trenutna_sekunda = 0;

		float* pcmData = (float*)malloc(PCM_BUFFER_SIZE);
		if (!pcmData) {
			drmp3_uninit(&mp3);
			reprodukcija_u_toku = false;
			return ENOMEM;
		}

		size_t totalFrames = 0;
		size_t maxFrames = PCM_BUFFER_SIZE / (mp3.channels * sizeof(float));
		uint16_t* pcmData16 = NULL;

		while (1) {
			size_t framesDecoded = drmp3_read_pcm_frames_f32(&mp3, maxFrames, pcmData);
			if (framesDecoded == 0) break;

			uint16_t* temp = (uint16_t*)realloc(pcmData16, (totalFrames + framesDecoded) * mp3.channels * sizeof(uint16_t));
			if (!temp) {
				free(pcmData);
				free(pcmData16);
				drmp3_uninit(&mp3);
				reprodukcija_u_toku = false;
				return ENOMEM;
			}
			pcmData16 = temp;

			for (size_t i = 0; i < framesDecoded * mp3.channels; i++) {
				float sample = pcmData[i] * 32767.0f;
				if (sample > 32767.0f) sample = 32767.0f;
				if (sample < -32768.0f) sample = -32768.0f;
				pcmData16[totalFrames * mp3.channels + i] = (uint16_t)((int16_t)sample);
			}
			totalFrames += framesDecoded;
		}

		free(pcmData);
		drmp3_uninit(&mp3);

		size_t dataSize = totalFrames * mp3.channels * sizeof(uint16_t);
		unsigned char* wavData = (unsigned char*)malloc(44 + dataSize);
		if (wavData) {
			unsigned char wavHeader[44] = {
				'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00, 'W', 'A', 'V', 'E',
				'f', 'm', 't', ' ', 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
				0x44, 0xAC, 0x00, 0x00, 0x10, 0xB1, 0x02, 0x00, 0x04, 0x00, 0x10, 0x00,
				'd', 'a', 't', 'a', 0x00, 0x00, 0x00, 0x00
			};
			size_t fileSize = dataSize + 36;
			wavHeader[4] = (unsigned char)(fileSize & 0xFF);
			wavHeader[5] = (unsigned char)((fileSize >> 8) & 0xFF);
			wavHeader[6] = (unsigned char)((fileSize >> 16) & 0xFF);
			wavHeader[7] = (unsigned char)((fileSize >> 24) & 0xFF);
			wavHeader[40] = (unsigned char)(dataSize & 0xFF);
			wavHeader[41] = (unsigned char)((dataSize >> 8) & 0xFF);
			wavHeader[42] = (unsigned char)((dataSize >> 16) & 0xFF);
			wavHeader[43] = (unsigned char)((dataSize >> 24) & 0xFF);

			memcpy(wavData, wavHeader, 44);
			memcpy(wavData + 44, pcmData16, dataSize);
			free(pcmData16);

			sviraj(wavData, 44 + dataSize);
			free(wavData);
		} else {
			free(pcmData16);
		}

		if (reprodukcija_u_toku) {
			trenutni_indeks = (trenutni_indeks + 1) % ukupan_broj_pesama;
			trenutni_fajl = plejlista[trenutni_indeks];
			osvezi_gui_nakon_automatskog_prelaza();
		}
	}

	reprodukcija_u_toku = false;
	reprodukcija_pauzirana = false;
	return EOK;
}


