//Importation des bibliothèques requises
#include <Arduino.h>    // Permet l'implémentation sous Arduino
#include <SPI.h>    // Permet la communication série périphérique
#include <SD.h> // Permet d'interagir avec une carte SD
#include "RTClib.h"     // Permet d'interagir avec des modules d'horloge
#include <Wire.h>   // Permet la communication I2C
#include <stdio.h>  // Permet les opérations entrées/sorties
#include <stdlib.h> // Permet la gestion de la mémoire dynamique
#include <SoftwareSerial.h> // Permet la configuration des ports de communication série loiciels supplémentaire
#include <ChainableLED.h>   // Permet de contrôler la LED
#include <Adafruit_Sensor.h>    // Permet d'interagir avec le capteur BMA280
#include <Adafruit_BME280.h>    // Permet d'interagir avec le capteur BMA280
#include <BME280I2C.h>  // Permet d'interagir avec le capteur BMA280 avec une interface I2C


RTC_DS3231 rtc; // Création d'un objet RTC_DS3231 appelé rtc
ChainableLED leds(5, 6, 1); // Création d'un objet leds avec les pins de la led
File dataFile;  // Création d'un objet dataFile pour lire ou écrire sur la carte SD
byte Mode = 0;  // Permet le changement de mode
BME280I2C bme;  // Création d'un objet bme pour interagir avec le capteur mesurant la température, l'humidité et la pression atmosphérique
// Définition des pins des boutons vert et rouge
#define PUSHPIN_V 2
#define PUSHPIN_R 3


// Création de la structure variables initialisant les valeurs par défaut des variables du programme
typedef struct variables{
    unsigned long log_interval = 100;   // Intervalle entre les mesures en millisecondes (10 minutes)
    unsigned short file_max_size = 2000;    // Taille maximale du fichier en octets (2 ko)
    unsigned short timeout = 10;    // Durée limite
    int currentRevision = 0;    // Stocke la révision actuelle
    volatile unsigned short start_time = 0; // Stocke le temps de démarrage
    bool lumin = 1; // Indique si le capteur de luminosité est actif
	bool temp_air = 1;  // Indique si le capteur de température est actif
    bool hygr = 1;  // Indique si le capteur d'hygrométrie est actif
    bool pressure = 1;  // Indique si le capteur de pression est actif
    // Indique la fourchette de valeur acceptable par la mesure de pression
    int pressure_min = 850; 
	int pressure_max = 1080;
    // Indique la fourchette de valeur acceptable par la mesure d'humidité
    int hygr_min = 0;
	int hygr_max = 50;
    // Indique la fourchette de valeur acceptable par la mesure de température
    int min_temp_air = -10;
	int max_temp_air = 60;
    // Indique la fourchette de valeur acceptable par la mesure de luminosité
    int lumin_low = 255;
	int lumin_high = 650;
}variables;

variables utile; // Initialisation d'une variable de type variables


// Création d'une structure pouvant stocker les données enregistrées
typedef struct {
    String date;    // Permet de stocker la date
    String humidity;// Permet de stocker la valeur d'humidité
	String pressure;// Permet de stocker la valeur de pression
	String temp;    // Permet de stocker la valeur de température
	String light;   // Permet de stocker la valeur de luminosité
} capteurs;

capteurs values; // Initialisation d'une variable de type capteurs


// Déclaration des prototypes de fonctions
void Rtc();
void light();
void initialisation_interruption();
void RedButton();
void GreenButton();
void M_standard();
void M_config();
void M_eco();
void M_maintenance();
string generateFileName(int revision);
int findCurrentRevision();
void verif();
void config();
void temperature();
void humidity();
void pression();


void setup(){
  Serial.begin(9600);   // Initialisation de la communication série
  Wire.begin(); // Initialisation de la communication I2C
  verif();  // Vérification de l'accès des capteurs et composants
  pinMode(PUSHPIN_R, INPUT); // Initialisation du bouton rouge
  pinMode(PUSHPIN_V, INPUT); // Initialisation du bouton vert
  initialisation_interruption();    // Initialisation des interruptions
  utile.currentRevision = findCurrentRevision();    // Initialisation de la revision actuelle
  // Si le bouton rouge est pressé, passer au mode configuration, sinon passer en mode standard
  if (digitalRead(PUSHPIN_R) == LOW or Mode == 1) {
  M_config();}
  else {M_standard();}}


void loop(){
  verif();  // Vérification des capteurs et composants
  switch (Mode) {
      case 4:   // Mode maintenance depuis le mode économique
        Rtc();    // Permet d'obtenir la date et l'heure
        light();  // Permet d'obtenir la valeur du capteur de luminosité
        temperature();    // Permet d'obtenir la valeur du capteur de température
        humidity();   // Permet d'obtenir la valeur du capteur d'humidité
        pression();   // Permet d'obtenir la valeur du capteur de pression
        // Affichage des valeurs
        Serial.print(values.date);
        Serial.println(values.light);
        Serial.print(values.date);
        Serial.print(values.light);
        Serial.print(values.temp);
        Serial.print(values.humidity);
        Serial.print(values.pressure);
        // Affichage de "N/A" suite à l'absence du capteur GPS
        Serial.println(F(" GPS:N/A"));
        break;

    case 3:   // Mode économique
        utile.log_interval = utile.log_interval*2;    //augmentation de l'intervalle entre mesures
        Rtc();
        light();
        temperature();
        humidity();
        pression();
        utile.log_interval = utile.log_interval / 2;  //diminution de l'intervalle entre mesures
        break;
	
    case 2: // Mode maintenance depuis le mode standard
        Rtc();
        light();
        temperature();
        humidity();
        pression();
        Serial.print(values.date);
        Serial.print(values.light);
        Serial.print(values.temp);
        Serial.print(values.humidity);
        Serial.print(values.pressure);
        Serial.println(F(" GPS:N/A"));
        break;
	
    case 1: // Mode configuration
        config();
        break;

    case 0: // Mode standard
        Rtc();
        light();
        temperature(); 
        humidity();
        pression();
		break;}

// Si l'on est en mode standard ou économique
  if (Mode == 0 || Mode == 3){
      //Ouvrir un fichier en mode écriture en le nommant avec la fonction generateFileName()
      dataFile = SD.open(generateFileName(utile.currentRevision), FILE_WRITE);
      // Si l'ouverture est réussi
      if (dataFile) {
          // Si la taille actuelle du fichier dépasse la valeur de utile.file_max_size
          if (dataFile.size() > utile.file_max_size) {
              dataFile.close(); // Fermez le fichier actuel
              utile.currentRevision = utile.currentRevision + 1;    // Incrémenter le numéro de révision de 1
              dataFile = SD.open(generateFileName(utile.currentRevision), FILE_WRITE);  // Création d'un nouveau fichier
              // Affichage du nom du nouveau fichier
              Serial.print(F("Creation d'un nouveau fichier : "));
              Serial.println(generateFileName(utile.currentRevision));}
          // Si le fichier est ouvert
          if (dataFile) {
              // Ecrire les valeurs des capteurs dans celui-ci
              dataFile.print(values.date);
              dataFile.print(values.light);
              dataFile.print(values.temp);
              dataFile.print(values.humidity);
              dataFile.print(values.pressure);
              dataFile.println(" GPS:N/A");
              dataFile.close(); // Fermer le fichier
              // Afficher les valeurs sur le port série
              Serial.print(values.date);
              Serial.print(values.light);
              Serial.print(values.temp);
              Serial.print(values.humidity);
              Serial.print(values.pressure);
              Serial.println(F(" GPS:N/A"));}
          // Si le fichier n'est pas ouvert, afficher une erreur
          else {
              while(1);}}
      delay(utile.log_interval);}}

// Fonction vérifiant l'accès aux capteurs et composants
void verif(){
    byte conteur = 0;
    while(!bme.begin()){    // Tant que l'initialisation du capteur BME échoue
        if (conteur >= utile.timeout) { // Si le délai d'attente maximal est atteint
            Serial.println(F("Échec"));
            while (1);}
        Serial.println(F("Erreur d'acces aux capteurs"));
        // Affichage de l'erreur sur la led
        leds.setColorRGB(0, 255, 0, 0);
        delay(400);
        leds.setColorRGB(0, 0, 255, 0);
        delay(400);
        conteur++;}

    conteur = 0;
    while (!rtc.begin()) {  // Tant que l'initialisation de l'horloge échoue
        if (conteur >= utile.timeout) {
            Serial.println(F("Échec"));
            while (1);}
        Serial.println(F("Erreur d'acces a l'horloge"));
        leds.setColorRGB(0, 255, 0, 0);
        delay(400);
        leds.setColorRGB(0, 0, 0, 255);
        delay(400);
        conteur++;} 

    conteur = 0;
    while (!SD.begin(4)) { // Tant que l'initialisation de la carte SD échoue
        if (conteur >= utile.timeout) {
            Serial.println(F("Échec"));
            while (1);}
        Serial.println(F("erreur d'acces a la SD"));
        leds.setColorRGB(0, 255, 0, 0);
        delay(300);
        leds.setColorRGB(0, 255, 255, 255);
        delay(600);
        conteur++;}

    switch (Mode) {
        case 4: // Afficher la led en orange
            leds.setColorRGB(0, 255, 70, 0); 
            break;

        case 3: // Afficher la led en bleu
            leds.setColorRGB(0, 0, 0, 255); 
	        break;

        case 2: // Afficher la led en orange
            leds.setColorRGB(0, 255, 70, 0);
	        break;

        case 1: // Afficher la led en jaune
            leds.setColorRGB(0, 255, 255, 0); 
            break;

        case 0: // Afficher la led en verte
            leds.setColorRGB(0, 0, 255, 0);
	        break;}}


// Mode configuration
void config(){
    unsigned long time = millis();  // Variable stockant le temps écoulé depuis l'activation de l'Arduino
    variables base;

    // Affichage sur le moniteur série
    Serial.println(F("Que souhaitez vous modifier :"));
    Serial.println(F("1-Intervalle des prises"));
    Serial.println(F("2-Taille des fichiers"));
    Serial.println(F("3-timeout"));
    Serial.println(F("4-reset"));
    Serial.println(F("5-autres paramètres"));

    // Si aucune données disponible sur le port série et que 30 secondes s'écoule
    while (!Serial.available() && millis() - time < 30000) {
        delay(500);}
    if (millis()-time > 30000){
        Serial.println(F("30 secondes d'inactivité, retour au mode standard"));
        Mode = 0;}  // Passer en mode standard
    
    // Création de variables
    String read = "";
    String read1 = "";
    short resultat;
    short commande;
    read1 = Serial.readString();
    commande = read1.toInt();
    // Permet de changer la valeur des différentes variables entre les différents cas affiché dans la console précédemment
    switch (commande){
        case 1:
            Serial.println(F("Entrer la valeur du nouvelle intervalle en sec"));
            while (!Serial.available()) delay(500);
		        read = Serial.readString();
                resultat = read.toInt();
                resultat = resultat *1000;
                utile.log_interval = resultat;
                Serial.print(F("nouvelle intervalle : "));
                Serial.println(utile.log_interval);
                break;

        case 2:
            Serial.println(F("Entrer la valeur de la taille du fichier en ko"));
            while (!Serial.available()) delay(500);
                read = Serial.readString();
                resultat = read.toInt();
                resultat = resultat *1000;
                utile.file_max_size = resultat;
                Serial.print(F("nouvelle taille : "));
                Serial.println(utile.file_max_size);
                break;

        case 3 :
            Serial.println(F("Entrer la valeur du time out en sec"));
            while (!Serial.available()) delay(500);
                read = Serial.readString();
                resultat = read.toInt();
                resultat = resultat *1024;
                utile.timeout = resultat;
                Serial.print(F("nouveau timeout : "));
                Serial.println(utile.timeout);
                break;

        case 4:
            Serial.println(F("Reinitialisation des valeurs par défaut"));
            utile.log_interval = base.log_interval;
			utile.file_max_size = base.file_max_size;
			utile.timeout = base.timeout;
			utile.lumin = base.lumin;
			utile.lumin_low = base.lumin_low;
			utile.lumin_high = base.lumin_high;
			utile.temp_air = base.temp_air;
			utile.min_temp_air = base.min_temp_air;
			utile.max_temp_air = base.max_temp_air;
			utile.hygr = base.hygr;
			utile.hygr_min = base.hygr_min;
			utile.hygr_max = base.hygr_max;
			utile.pressure = base.pressure;
			utile.pressure_min = base.pressure_min;
			utile.pressure_max = base.pressure_max;
			Serial.println(F("Default values restored"));
            break;

        case 5:
            Serial.println(F("LUMIN. Choices : \n1) True\n2) False"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			if (read == "1") utile.lumin=1;
			else if (read == "2") utile.lumin = 0;
            Serial.println(utile.lumin);
            Serial.println(F("LUMIN_LOW (D=255)"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			utile.lumin_low = read.toInt();
            Serial.println(utile.lumin_low);
            Serial.println(F("LUMIN_HIGH (D=665)"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			utile.lumin_high = read.toInt();
            Serial.println(utile.lumin_high);
            Serial.println(F("TEMP_AIR . Choices : \n1) True\n2) False"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			if(read == "1") utile.temp_air = 1;
			else if (read == "2") utile.temp_air = 0;
            Serial.println(utile.temp_air);
            Serial.println(F("MIN_TEMP_AIR (D=-10)"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			utile.min_temp_air = read.toInt();
            Serial.println(utile.min_temp_air);
            Serial.println(F("MAX_TEMP_AIR (D=60)"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			utile.max_temp_air = read.toInt();
            Serial.println(utile.max_temp_air);
            Serial.println(F("HYGR. Choices : \n1) True\n2) False"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			if(read == "1") utile.hygr = 1;
			else if (read == "2") utile.hygr = 0;
            Serial.println(utile.hygr);
            Serial.println(F("HYGR_MINT (D=0)"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			utile.hygr_min = read.toInt();
            Serial.println(utile.hygr_min);
            Serial.println(F("HYGR_MAXT (D=50)"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			utile.hygr_max = read.toInt();
            Serial.println(utile.hygr_max);
            Serial.println(F("PRESSURE: \n1) True\n2) False"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			if(read == "1") utile.pressure = 1;
			else if (read == "2") utile.pressure = 0;
            Serial.println(utile.pressure);
            Serial.println(F("PRESSURE_MIN (D=850)"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			utile.pressure_min = read.toInt();
            Serial.println(utile.pressure_min);
            Serial.println(F("PRESSURE_MAX (D=1080)"));
			while (!Serial.available()) delay(500);
			read = Serial.readString();
			utile.pressure_max = read.toInt();
            Serial.println(utile.pressure_max);
            break;}
    delay(5000);}

// Fonction du mode standard
void M_standard() {
    Mode = 0;
    Serial.println(F("Mode standard"));
    leds.setColorRGB(0, 0, 255, 0);}

// Fonction du mode économique
void M_eco(){
    Mode = 3;
    Serial.println(F("Mode eco"));
    leds.setColorRGB(0, 0, 0, 255);}

// Fonction du mode maintenance
void M_maintenance(){
    Serial.println(F("Mode maintenance"));
    leds.setColorRGB(0, 255, 70, 0);}

// Fonction du mode configuration
void M_config(){
    Mode =1;
    Serial.println(F("Mode configuration"));
    leds.setColorRGB(0, 255, 255, 0); }

// Fonction de l'horloge
void Rtc(){
    DateTime now = rtc.now();   // Obtention de la date et l'heure de l'horloge
    now = now + TimeSpan(8, 3, 49, -30);    // Actualisation de l'heure
    // Création d'une chaîne de caractère contenant la date et l'heure
    String timestamp = "";
    timestamp += now.day();
    timestamp += "/";
    timestamp += now.month();
    timestamp += "/";
    timestamp += now.year();
    timestamp += "  ";
    timestamp += now.hour();
    timestamp += ":";
    timestamp += now.minute();
    timestamp += ":";
    timestamp += now.second();
    timestamp += " ; ";
    values.date = timestamp;}

// Fonction du capteur de luminosité
void light(){
    if (utile.lumin){
        int sensor = analogRead(A2);    // Lecture analogique sur le port de branchement du capteur
        if (sensor >= 290 && sensor <= 310) {   // Si la valeur mesuré ne se trouve pas dans la plage acceptable
            values.light = "light= NA  ; ";}
        else {  // Sinon on stocke la valeur
            values.light = "light:" + String(sensor);}
        // Ajout de L ou H à la fin de la mesure
        if (sensor<utile.lumin_low){    // LOW si la luminosité est basse
            values.light += "L ; ";}
        else if (sensor>utile.lumin_high){  // HIGH si elle est haute
            values.light += "H ; ";}
        else {values.light += " ; ";}}}

// Fonction du capteur de température
void temperature(){
    if (utile.temp_air){
        float temps(NAN), hum(NAN), pres(NAN);
        // Déclaration des unités de mesures
        BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
        BME280::PresUnit presUnit(BME280::PresUnit_Pa);
        bme.read(pres, temps, hum, tempUnit, presUnit);
        values.temp = "temp:" + String(temps) + "°C";
        if (temps<utile.min_temp_air){
            values.temp += "L ; ";}
        else if (temps>utile.max_temp_air){
            values.temp += "H ; ";}
        else {
            values.temp += " ; ";}}}

// Fonction du capteur d'humidité
void humidity(){
    if (utile.hygr){
        float temps(NAN), hum(NAN), pres(NAN);
        bme.read(pres, temps, hum);
        values.humidity = "hum:" + String(hum) + "%";
        if (hum<utile.hygr_min){
            values.humidity += "L ; ";}
        else if (temps>utile.hygr_max){
            values.humidity += "H ; ";}
        else {
            values.humidity += " ; ";}}}

// Fonction du capteur de pression
void pression(){
    if (utile.pressure){
        float temps(NAN), hum(NAN), pres(NAN);
        bme.read(pres, temps, hum);
        values.pressure = "pres:" + String(pres) + "PA";
        if (pres<utile.pressure_min){
            values.pressure += "L ; ";}
        else if (pres>utile.pressure_max){
            values.pressure += "H ; ";}
        else {
            values.pressure += " ; ";}}}

// Fonction générant le nom de fichier
String generateFileName(int revision){
    DateTime now = rtc.now(); // Obtention de la date et l'heure  
    String filename;
    // Mise en forme des valeurs
    int ans = now.year() % 100;
    int mois = now.month() +1;
    int jour = now.day() -23;
    // Création du nom de fichier à partir de la date, l'heure et le numéro de révision
    filename += String(ans) + String(mois) + String(jour) + "_" + String(revision, DEC) + ".txt";
    return filename;}

// Fonction générant le numéro de révision
int findCurrentRevision() {
    int maxRevision = 0;
    // Tant qu'il existe un fichier avec la révision actuelle
    while (SD.exists(generateFileName(maxRevision))) {
        maxRevision++;} // Incrémenter le numéro de révision
    // Si le numéro de révision est supérieur à 0 et que la taille du fichier avec le numéro de révision précédent a une taille inférieure à celle autorisé
    if (maxRevision>0 && (SD.open(generateFileName(maxRevision-1), FILE_WRITE)).size() < utile.file_max_size){
        maxRevision--;} // Décrémenter le numéro de révision
    return maxRevision;}

// Fonction initialisant les interruptions
void initialisation_interruption() {
    // Initialisation de l'interruption du bouton rouge en appelant la fonction RedButton()
    attachInterrupt(digitalPinToInterrupt(PUSHPIN_R), RedButton, CHANGE); 
    // Initialisation de l'interruption du bouton vert en appelant la fonction GreenButton()
    attachInterrupt(digitalPinToInterrupt(PUSHPIN_V), GreenButton, CHANGE);}

// Fonction d'interruption du bouton rouge
void RedButton(){
    if (utile.start_time == 0){ // Si le bouton d'interruption est pressé pour la première fois
        utile.start_time = millis();}   // Initialiser start_time à millis()
    else if (10 < millis() - utile.start_time){ // Si le bouton a été pressé plus de 10 secondes
        // Afficher le temps de pression en ms
        Serial.print(F("Red Button pressed :"));
        Serial.print(millis() - utile.start_time);
        Serial.println(F("ms"));
        if ( millis() - utile.start_time >= 5000){  // Si le bouton a été pressé plus de 5 secondes
            utile.start_time = 0;
            // Effectuer le changement de mode correspondant
            switch(Mode){
                case 0:
                    Mode = 2;
                    M_maintenance();
                    break;
          
                case 2:
                    M_standard();
                    break;
          
                case 4:
                    M_eco();
                    break;

                case 3:
                    Mode = 4;
                    M_maintenance();
                    break;}}
        utile.start_time = 0;}}

// Fonction d'interruption du bouton vert
void GreenButton(){
    if (utile.start_time == 0){
        utile.start_time = millis();}
    else if (10 < millis() - utile.start_time){
        Serial.print(F("Green Button pressed :"));
        Serial.print(millis() - utile.start_time);
        Serial.println(F("ms"));
        if ( millis() - utile.start_time >= 5000){
        utile.start_time = 0;
        switch(Mode){
            case 0:
                M_eco();
                break;
        
            case 3:
                M_standard();
                break;}}
        utile.start_time = 0;}}