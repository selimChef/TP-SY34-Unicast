/**
 * TP SY34 
 * @author Florent Gertaldi, Sélim Chefchaouni, Lydie Taupin
 */

// PIC18F46J50 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1L
#pragma config WDTEN = OFF // Watchdog Timer (Enabled)
#pragma config PLLDIV = 2  // PLL Prescaler Selection bits (No prescale (4 MHz oscillator input drives PLL directly))
#pragma config STVREN = ON // Stack Overflow/Underflow Reset (Enabled)
#pragma config XINST = OFF // Extended Instruction Set (Enabled)

// CONFIG1H
#pragma config CPUDIV = OSC3_PLL3 // CPU System Clock Postscaler (No CPU system clock divide)
#pragma config CP0 = OFF          // Code Protect (Program memory is not code-protected)

// CONFIG2L
#pragma config OSC = HSPLL   // Oscillator (HS+PLL, USB-HS+PLL)
#pragma config T1DIG = ON    // T1OSCEN Enforcement (Secondary Oscillator clock source may be selected)
#pragma config LPT1OSC = OFF // Low-Power Timer1 Oscillator (High-power operation)
#pragma config FCMEN = ON    // Fail-Safe Clock Monitor (Enabled)
#pragma config IESO = ON     // Internal External Oscillator Switch Over Mode (Enabled)

// CONFIG2H
#pragma config WDTPS = 32768 // Watchdog Postscaler (1:32768)

// CONFIG3L
#pragma config DSWDTOSC = INTOSCREF // DSWDT Clock Select (DSWDT uses INTRC)
#pragma config RTCOSC = T1OSCREF    // RTCC Clock Select (RTCC uses T1OSC/T1CKI)
#pragma config DSBOREN = ON         // Deep Sleep BOR (Enabled)
#pragma config DSWDTEN = ON         // Deep Sleep Watchdog Timer (Enabled)
#pragma config DSWDTPS = G2         // Deep Sleep Watchdog Postscaler (1:2,147,483,648 (25.7 days))

// CONFIG3H
#pragma config IOL1WAY = ON     // IOLOCK One-Way Set Enable bit (The IOLOCK bit (PPSCON<0>) can be set once)
#pragma config MSSP7B_EN = MSK7 // MSSP address masking (7 Bit address masking mode)

// CONFIG4L
#pragma config WPFP = PAGE_63    // Write/Erase Protect Page Start/End Location (Write Protect Program Flash Page 63)
#pragma config WPEND = PAGE_WPFP // Write/Erase Protect Region Select (valid when WPDIS = 0) (Page WPFP<5:0> through Configuration Words erase/write protected)
#pragma config WPCFG = OFF       // Write/Erase Protect Configuration Region (Configuration Words page not erase/write-protected)

// CONFIG4H
#pragma config WPDIS = OFF // Write Protect Disable bit (WPFP<5:0>/WPEND region ignored)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.


/****************************************************/
/*                   Headers                        */
/****************************************************/
#include <xc.h>
#include "VT100.h"
#include <string.h>
#include "system.h"
#include "system_config.h"
#include "miwi/miwi_api.h"
#include <stdarg.h>
#include "lcd.h"


/****************************************************/
/*                   Variables                      */
/****************************************************/
extern API_UINT16_UNION myPANID;                                      // mon PANID
extern uint8_t myLongAddress[MY_ADDRESS_LENGTH];                      // mon adresse IEEE
extern API_UINT16_UNION myShortAddress;                               // mon adresse logique
extern ACTIVE_SCAN_RESULT ActiveScanResults[ACTIVE_SCAN_RESULT_SIZE]; // table des adresses présentes

#define PSEUDO_MAX_LENGTH 8     // taille max pseudo
#define MY_CHANNEL 14           // channel du réseau MiWi (doit être identique pour les trois terminaux)
#define MESSAGE_LENGTH 50       // taille d'un message

#define DATA 0xAA
#define CMD 0x55

extern RECEIVED_MESSAGE rxMessage;

char myPseudo[PSEUDO_MAX_LENGTH + 1];
int countRB0, countRB2;
bool state2, prevState2, state0, prevState0;


/****************************************************/
/*                   Prototypes                     */
/****************************************************/
void getPseudo(char *);
void RX(void);
void TX(void);
void broadcastData(char *, ...);
void initChat(void);
void initNwk(void);
void sendBroadcast(char *pseudo, int counter);
void sendUnicast(char *pseudo, int counter, char buttonPushed);


/****************************************************/
/*                   Programme                      */
/****************************************************/

/**
 * Éxécution sur le microcontrôleur
 */
void main(void)
{
    // Initialisation Carte
    SYSTEM_Initialize();

    // Initialisation Uart
    uartInitialize();

    // Identification
    initChat();

    // Connexion réseau
    initNwk();
    
    // Boucle réception/transmission
    while (1)
    {
        RX();
        TX();
    }
}

/**
 * Initialisation du chat : message d'invite et acquisition du pseudo
 */
void initChat(void)
{
#ifdef NO_TERM
    LCDBacklightON();
    memcpy(myPseudo, "bot", strlen("bot") + 1);
    LCDDisplay((char *)"Pseudo : bot", 0, true);
#else
    vT100CleanScreen();
    vT100PlaceCursor(0, 0);
    uartPrint("Saisissez votre pseudo : ");
    getPseudo(myPseudo);
    uartPrint("\r\n Bonjour : ");
    uartPrint(myPseudo);
    uartPrint("!\r\n");
#endif
}

/**
 * Initialisation du réseau
 */
void initNwk()
{
    uint8_t respondingDevices;
    bool found;
    uint8_t index;

    MiApp_ProtocolInit(false);

    if (MiApp_SetChannel(MY_CHANNEL) == false)
    { // Réglage canal 11
        #ifdef NO_TERM
            LCDDisplay((char *)"Err selection canal", 0, true);
        #else
            uartPrint("Erreur : selection de canal");
        #endif
        goto fin;
    }

    respondingDevices = MiApp_SearchConnection(10, 1L << MY_CHANNEL);
    found = false;
    if (respondingDevices != 0)
    {
        for (index = 0; index < respondingDevices; index++)
        {
            if (found = (ActiveScanResults[index].PANID.Val == MY_PAN_ID))
                //toto = index;
                break;
        }
    }
    // found existing PAN controller
    if (found)
    {
        MiApp_ConnectionMode(ENABLE_ACTIVE_SCAN_RSP);
        if (MiApp_EstablishConnection(index, CONN_MODE_DIRECT) == 0xFF)
        {
            #ifdef NO_TERM
                LCDDisplay((char *)"Err connexion refusee", 0, true);
            #else
                uartPrint("Erreur : connexion refusee");
            #endif
            goto fin;
        }
        #ifdef NO_TERM
            LCDDisplay((char *)"Connexion OK", 0, true);
        #else
            uartPrint("Connexion reussie sur PAN existant\n\r");
        #endif
        // nobody
    }
    else
    {
        MiApp_ConnectionMode(ENABLE_ALL_CONN);
        if (!MiApp_StartConnection(START_CONN_DIRECT, 0, 0))
        {
        #ifdef NO_TERM
            LCDDisplay((char *)"Er : conn. refusee", 0, true);
        #else
            uartPrint("Erreur : creation refusee");
        #endif
            goto fin;
        }
        #ifdef NO_TERM
            LCDDisplay((char *)"Nouveau PAN !", 0, true);
        #else
            uartPrint("Creation d'un nouveau PAN\n\r");
        #endif
    }
    #ifdef NO_TERM
        sprintf(LCDText, "Adresse : 0X%04x", myShortAddress);
        sprintf(&LCDText[16], "sur PAN : 0X%04x", MY_PAN_ID);
        LCD_Update();
    #else
        uartPrint("Votre adresse est : 0x");
        uartHexaPrint((uint8_t *)&myShortAddress, 2);
        uartPrint("\r\n");
    #endif

    return;
fin:
    while (1)
        ;
}

/**
 * Récupération du pseudo de l'utilisateur
 * @param pseudo : une table de 9 octets contenant le pseudo terminé par un 0 
 */
void getPseudo(char *pseudo)
{
    int i = 0;
    do
    {
        if (uartIsChar())
        {
            pseudo[i++] = uartRead();
        }
    } while ((pseudo[i - 1] != 0x0D) || (i >= PSEUDO_MAX_LENGTH));
    pseudo[i - 1] = 0;
}

/**
 * Gestion des messages entrants
 */
void RX(void)
{
    if (MiApp_MessageAvailable())
    {
        uartPrint(rxMessage.Payload);
        MiApp_DiscardMessage();
    }
}

/**
 * Gestion des messages sortants
 */
void TX(void)
{
    state2 = PORTBbits.RB2;
    if(!state2 && prevState2 && countRB2 < 99){
        countRB2++;
        sendUnicast(myPseudo, countRB2, 2);
    }
    prevState2 = state2;

    state0 = PORTBbits.RB0;
    if(!state0 && prevState0 && countRB0 < 99){
        countRB0++;
        sendUnicast(myPseudo, countRB0, 0);
    }
    prevState0 = state0;
}

/**
 * Envoi d'un message en unicast
 * @param pseudo : pseudo de l'utilisateur qui envoie le message
 * @param counter : compteur du nombre de messages envoyés
 * @param buttonPushed : bouton utilisé pour l'envoi (0 pour RB0, 2 pour RB2)
 */
void sendUnicast(char *pseudo, int counter, char buttonPushed)
{
    char msg[MESSAGE_LENGTH];           // message à envoyer
    int i = 0;                          // déplacement dans le message
    int myAdress = myShortAddress.Val;  // adresse du terminal
    uint16_t destinationAdress;         // adresse du terminal de destination 

    sprintf(msg, "\r\nPseudo : %s, message unicast : %d", pseudo, counter);
    MiApp_FlushTx();
    while (msg[i] != '\0')
    {
        MiApp_WriteData(msg[i]);
        i++;
    }
    MiApp_WriteData('\0');

    char msgUnit[100];
    uartPrint(msgUnit);

    // définition adresse de destination 
    if (buttonPushed == 0)
    {
        if (myAdress == 0)
        {
            destinationAdress = 0x0100;
        }
        else
        {
            destinationAdress = 0x0000;
        }
    }
    else
    {
        if (myAdress == 200)
        {
            destinationAdress = 0x0100;
        }
        else
        {
            destinationAdress = 0x0200;
        }
    }

    sprintf(msgUnit, "\r\nAdresse d'envoi : 0x %4x", destinationAdress);
    
    // envoi du message en unicast
    MiApp_UnicastAddress((uint8_t *)&destinationAdress, false, false); 
}