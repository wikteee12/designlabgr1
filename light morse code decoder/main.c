#include "MKL05Z4.h"
#include "ADC.h"
#include "frdm_bsp.h"
#include "lcd1602.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


volatile int wyraz_i = 0;
volatile float prog_tla = 1;
volatile float adc_volt_coeff = ((float)(((float)2.91) / 4096));
volatile uint8_t wynik_ok = 0;
volatile uint16_t temp;
volatile float wynik;


volatile uint32_t timer_ms = 0;
volatile int jednostkaczasu_ms = 1000; // Czas kropki
volatile int tolerancja_ms = 500;     // Tolerancja błędu

char wyraz[] = { 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20 };
char kod[] = { 0x20,0x20,0x20,0x20,0x20,0x20 };
char display[] = { 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20 };
volatile int czas_trwania_h = 0;
volatile int czas_trwania_l = 0;


void PIT_Init(void) {
	SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;
	PIT->MCR &= ~PIT_MCR_MDIS_MASK;
	PIT->CHANNEL[0].LDVAL = ((SystemCoreClock / 2) / 1000) - 1;
	PIT->CHANNEL[0].TCTRL |= PIT_TCTRL_TIE_MASK | PIT_TCTRL_TEN_MASK;
	NVIC_ClearPendingIRQ(PIT_IRQn);
	NVIC_EnableIRQ(PIT_IRQn);
}

void PIT_IRQHandler(void) {
	if (PIT->CHANNEL[0].TFLG & PIT_TFLG_TIF_MASK) {
		PIT->CHANNEL[0].TFLG = PIT_TFLG_TIF_MASK;
		timer_ms++;
	}
}

void ADC0_IRQHandler()
{
	temp = ADC0->R[0];
	if (!wynik_ok) {
		wynik = temp;
		wynik_ok = 1;
	}

}

char dekoduj_znak(char* kod) {
	if (strcmp(kod, ".-") == 0) return 'A';
	if (strcmp(kod, "-...") == 0) return 'B';
	if (strcmp(kod, "-.-.") == 0) return 'C';
	if (strcmp(kod, "-..") == 0) return 'D';
	if (strcmp(kod, ".") == 0) return 'E';
	if (strcmp(kod, "..-.") == 0) return 'F';
	if (strcmp(kod, "--.") == 0) return 'G';
	if (strcmp(kod, "....") == 0) return 'H';
	if (strcmp(kod, "..") == 0) return 'I';
	if (strcmp(kod, ".---") == 0) return 'J';
	if (strcmp(kod, "-.-") == 0) return 'K';
	if (strcmp(kod, ".-..") == 0) return 'L';
	if (strcmp(kod, "--") == 0) return 'M';
	if (strcmp(kod, "-.") == 0) return 'N';
	if (strcmp(kod, "---") == 0) return 'O';
	if (strcmp(kod, ".--.") == 0) return 'P';
	if (strcmp(kod, "--.-") == 0) return 'Q';
	if (strcmp(kod, ".-.") == 0) return 'R';
	if (strcmp(kod, "...") == 0) return 'S';
	if (strcmp(kod, "-") == 0) return 'T';
	if (strcmp(kod, "..-") == 0) return 'U';
	if (strcmp(kod, "...-") == 0) return 'V';
	if (strcmp(kod, ".--") == 0) return 'W';
	if (strcmp(kod, "-..-") == 0) return 'X';
	if (strcmp(kod, "-.--") == 0) return 'Y';
	if (strcmp(kod, "--..") == 0) return 'Z';


	if (strcmp(kod, ".----") == 0) return '1';
	if (strcmp(kod, "..---") == 0) return '2';
	if (strcmp(kod, "...--") == 0) return '3';
	if (strcmp(kod, "....-") == 0) return '4';
	if (strcmp(kod, ".....") == 0) return '5';
	if (strcmp(kod, "-....") == 0) return '6';
	if (strcmp(kod, "--...") == 0) return '7';
	if (strcmp(kod, "---..") == 0) return '8';
	if (strcmp(kod, "----.") == 0) return '9';
	if (strcmp(kod, "-----") == 0) return '0';

	return '?'; // Nieznany znak
}

void kalibracja(void)
{
	while (!wynik_ok)
	{
		wynik_ok = 0;

		while (wynik * adc_volt_coeff >= 2)
		{
			LCD1602_SetCursor(0, 0);
			LCD1602_Print("                ");
			LCD1602_SetCursor(0, 0);
			sprintf(display, "Za jasno: %.2fV", wynik * adc_volt_coeff);
			LCD1602_Print(display);
			DELAY(2000); // aby nie migało za często
			while (!wynik_ok); // czekaj aż ADC zrobi nowy pomiar
			wynik_ok = 0;
		}

		prog_tla = wynik * adc_volt_coeff;


	}
}

void measure_high(void) {
	int start = timer_ms;


	while (1) {
		if (wynik_ok)
		{
			wynik_ok = 0;

			if ((wynik * adc_volt_coeff) < prog_tla + 0.25)
			{
				break;

			}

			if ((timer_ms - start) > 5 * jednostkaczasu_ms) {
				kalibracja();
				break;
			}

		}
	}
	czas_trwania_h = timer_ms - start;
}


void measure_low(void)
{
	int start = timer_ms;


	while (1)
	{
		if (wynik_ok)
		{
			wynik_ok = 0;

			if ((wynik * adc_volt_coeff) >= prog_tla + 0.5) break;
		}


		if ((timer_ms - start) > (3.01 * jednostkaczasu_ms))
		{
			kalibracja();
			czas_trwania_l = timer_ms - start;
			break;
		}
	}
	czas_trwania_l = timer_ms - start;

}


int main(void)
{

	LCD1602_Init();
	LCD1602_Backlight(TRUE);
	LCD1602_Print("Gotowy...");
	PIT_Init();

	if (ADC_Init()) { LCD1602_Print("Blad ADC"); while (1); }

	// Start ADC
	ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(8);
	DELAY(1000); 
	kalibracja();

	while (1)
	{



		if (wyraz_i > 16) //Sprawdzanie dlugosci slowa jesli za dlugie wyczysc tablice
		{
			LCD1602_SetCursor(0, 1);
			LCD1602_Print("                ");
			LCD1602_SetCursor(0, 1);
			LCD1602_Print("Za dlugi wyraz");
			for (int i = 0; i < 16; i++)
			{
				wyraz[i] = 20;
			}
			wyraz_i = 0;
		}


		if (wynik_ok)  //Pomiar stanu niskiego co obrót pętli dlatego że umożliwia nam to bezpieczną kalibracje co 3s

		{
			wynik_ok = 0;
			measure_low();
		}

		if (czas_trwania_l > 3 * jednostkaczasu_ms && wyraz_i > 0) //Jeśli przez 3 s nie zaczelismy znaku to wypisz wynik to i wyczysc tablice
		{

			LCD1602_SetCursor(0, 0);
			LCD1602_Print("                ");
			LCD1602_SetCursor(0, 0);
			LCD1602_Print("Wynik to: ");
			for (int i = 0; i < 16; i++)
			{
				wyraz[i] = 20;
			}
			wyraz_i = 0;

		}


		if (wynik_ok)
		{
			wynik_ok = 0;


			if ((wynik * adc_volt_coeff) >= prog_tla + 0.5) //czekamy aż pojawi sie stan wysoki aby zaczac dekodowanie znaku
			{

				for (int i = 0; i < 5; i++) //czyścimy tablice znaków odebranych 
				{
					kod[i] = 32;
				}

				uint8_t i = 1;
				measure_high(); //mierzymy ile ten stan wysoki trwa



				if (czas_trwania_h > (jednostkaczasu_ms - tolerancja_ms) && czas_trwania_h < (jednostkaczasu_ms + tolerancja_ms)) {
					//dekodowanie znaku na podstawie czasu trwania stanu wysokiego

					if (wyraz_i == 0)
					{
						LCD1602_ClearAll(); //jeśli to 1 znak wyczysc wyswietlacz
					}
					else
					{
						LCD1602_SetCursor(0, 0);
						LCD1602_Print("                ");

					}

					DELAY(1000);
					LCD1602_SetCursor(0, 0);
					LCD1602_Print("Odebrany znak: .");
					kod[0] = 46;
				}
				else if (czas_trwania_h > (3 * jednostkaczasu_ms - tolerancja_ms) && czas_trwania_h < (3 * jednostkaczasu_ms + tolerancja_ms)) {

					if (wyraz_i == 0)
					{
						LCD1602_ClearAll(); //jeśli to 1 znak wyczysc wyswieltacz
					}
					else
					{
						LCD1602_SetCursor(0, 0);
						LCD1602_Print("                "); //jak nie to tylko gorna część

					}

					DELAY(1000);
					LCD1602_SetCursor(0, 0);
					LCD1602_Print("Odebrany znak: -");
					kod[0] = 45;
				}
				//jeżeli długość czasu nie pasuje nigdzie to błąd i znak nie zostaje rozpoczęty
				else {
					LCD1602_SetCursor(0, 0);
					LCD1602_Print("                ");
					LCD1602_SetCursor(0, 0);
					sprintf(display, "Err1: %dms", czas_trwania_h);

					LCD1602_Print(display);

					continue;
				}


				while (1)
				{
					//jeśli uda znaleźć się 1 znak to szukamy kolejnych

					if (i > sizeof(kod) / sizeof(char) - 1) //jeśli jest więcej niż 5 kropek/kresek to anuluj znak 
					{
						LCD1602_SetCursor(0, 0);
						LCD1602_Print("                ");
						LCD1602_SetCursor(0, 0);
						LCD1602_Print("Za dlugi kod");
						break;
					}


					measure_low(); //sprawdzamy ile trwa przerwa między nadawaniem jeśli więcej niż 3s zakończ znak i wyczyść co trzeba


					if (czas_trwania_l >= 3 * jednostkaczasu_ms) {
						LCD1602_SetCursor(0, 1);
						kod[i] = 0;

						if (dekoduj_znak(kod) == '?') //jeśli żaden znak nie pasuje do kombinacji to nie dodawaj nic do wyrazu
						{
							LCD1602_SetCursor(0, 1);
							LCD1602_Print(wyraz);
							LCD1602_SetCursor(0, 0);
							LCD1602_Print("                ");
							LCD1602_SetCursor(0, 0);
							LCD1602_Print("Brak znaku");
							break;
						}

						wyraz[wyraz_i] = dekoduj_znak(kod); //jeśli znaleziono znak dodaj do wyrazu i wyświetl
						wyraz_i++;
						LCD1602_SetCursor(0, 1);
						LCD1602_Print(wyraz);



						break;
					}


					measure_high(); //jeżeli zaświecono latarką dekoduj kolejne znaki jak wcześniej




					if (czas_trwania_h > (jednostkaczasu_ms - tolerancja_ms) && czas_trwania_h < (jednostkaczasu_ms + tolerancja_ms)) {
						LCD1602_SetCursor(0, 0);
						LCD1602_Print("                ");
						DELAY(1000);
						LCD1602_SetCursor(0, 0);
						LCD1602_Print("Odebrany znak: .");
						kod[i] = 46;
					}
					else if (czas_trwania_h > (3 * jednostkaczasu_ms - tolerancja_ms) && czas_trwania_h < (3 * jednostkaczasu_ms + tolerancja_ms)) {
						LCD1602_SetCursor(0, 0);
						LCD1602_Print("                ");
						DELAY(1000);
						LCD1602_SetCursor(0, 0);
						LCD1602_Print("Odebrany znak: -");
						kod[i] = 45;
					}
					else
						//jeśli nie zmieszczono się wramy czasowe to podaj kropke kreske jeszcze raz
					{
						LCD1602_SetCursor(0, 0);
						LCD1602_Print("                ");
						LCD1602_SetCursor(0, 0);
						sprintf(display, "Err2: %dms", czas_trwania_h);

						LCD1602_Print(display);


						continue;
					}
					i++;
				}
			}
		}

	}
}