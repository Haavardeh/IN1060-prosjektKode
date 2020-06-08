/*
Dette programmet bruker en fototransistor til aa justere LED-lys paa en installasjon.
Sensorens innverdier bestemmer hvilke ledlys som skal begynne å lyse med animasjoner.
*/

// Definerer alle ledpaerene ved den endelige loesningen (int-verdi er fysisk port paa arduinoen)
  // Groenne led-lys fra 3-6, roede lys fra 8-11
    const byte gronnLed[] = {3,4,5,6};
    const byte rodLed[] = {8,9,10,11};

// Herfra begynner registreringen av phototransistoren
  const int photoTran = A0;                 // Fototransistor
  const int tiltSens = 2;                   // Tilt-sensor

// Variabler
  String utskriftstekst = ""; // til intern utskrift

  // Sensor-essensielle
  float sensorReading = 0;
  int tiltReading = 0;
  int buttonState = 0;

  // Sensor-min max og kalibrering
  float sensorHoyesteReading = 0;
  float sensorMinsteReading = 0;

  // Variabler rundt gjennomsnitt av sensor
  float sensorGjennomsnittsReading = 0;
  int sensorAvlesningerFoerGjennomsnitt = 10;
  int sensorArray[10];
  int avlesningNummer = 0;
  float midlertidigSum = 0;

  // Kalibrering (optional)
  unsigned long forrigeKalibreringsTidspunkt = 0;
  unsigned long forrigeSensorSjekkTidspunkt = 0;
  unsigned long forrigeAnimasjonsTidspunkt = 0;

  // Positiv brikke og lys-animasjon-variabler
  int sensorLysStatus = 0;                    // 0 er full moerke, 1 er halvveis dekt, 2 er lys
  bool erSisteBrikkePositivStatus = false;
  bool erSisteBrikkeNegativStatus = false;
  bool erPositivAnimasjonAktiv = false;
  bool erNegativAnimasjonAktiv = false;
  int positivAnimasjonsFrame = 0;
  int negativAnimasjonsFrame = 0;
  int positivAnimasjonMaxFrameLengde = 4;     // Maa justeres ved endring av animasjonslengde
  int negativAnimasjonMaxFrameLengde = 4;     // Maa justeres ved endring av animasjonslengde
  int roedtLysDelay = 0;

  // Lysfoelsomhet og kalibrering
  int roedGrenseVerdi = 150;                  // Ideell mellom 50 og 150? Denne verdien bestemmer naar loesningen skal skru seg selv av. (ingen brikke plassert)
  int groennGrenseVerdi = 40;                 // pluss paa litt for sterkt lys, blir vel aldri hoyere enn hundre, ever?
                                              // Denne verdien er nødt til å være så høy som den må i de lyseste lysforhold, ellers er jo mindre jo bedre.
  float kalibreringsMultiplier = 4.2;

// Timere, justerbare
  int millisMellomHverKalibrering = 2000;     // Millisekunder mellom hver sensorkalibrering ("refresh rate")
  int millisMellomHverSensorSjekk = 100;
  int millisMellomHverAnimasjon = 300;

// Utskrift som kan brukes i while-loops uten repetisjon.
  void utskrift(String tekst) {
    if (tekst != utskriftstekst) {
      Serial.println(tekst);
      utskriftstekst = tekst;
    }
  }

// Arduino Setup
void setup() {
  // Serial
  Serial.begin(9600);

  // LED-outputs
    // Roede LEDs
    for(byte i = 0; i < sizeof(rodLed); i++){
      pinMode(rodLed[i], OUTPUT);
    }
    // Groenne LEDs
    for(byte i = 0; i < sizeof(gronnLed); i++){
      pinMode(gronnLed[i], OUTPUT);
    }

  // LED-defaults paa setup
    // Roede LEDs
    for(byte i = 0; i < sizeof(rodLed); i++){
      digitalWrite(rodLed[i], LOW);
    }
    // Groenne LEDs
    for(byte i = 0; i < sizeof(gronnLed); i++){
      digitalWrite(gronnLed[i], LOW);
    }

    // Fototransistor og tiltsensor-pinModes
    pinMode(photoTran, INPUT);
    pinMode(tiltSens, INPUT);
}

void loop() {

  /* Autokalibrering hvert x. sekund. Hvis lysmiljoet endrer seg over tid, vil
    kalibreringen ha mulighet til aa endre seg over tid.
    Dette fungerer ikke noedvendigvis 100% i veldig moerke miljoer eller med skygge.
    Slik vil stoerre endringer og feil bli justert for over en viss tidsperiode av seg selv.
    Alt dette vil inngaa i kalibrer();
  */

    if ((millis() - forrigeKalibreringsTidspunkt) > millisMellomHverKalibrering) {
      kalibrer();
    }


  lesSensor();   // Sensoren leses av kontinuerlig som ledd i aa finne gjennomsnittsverdien.

/*
Koden under kjoerer for aa bestemme hvilken status brettet skal vaere i basert paa gjennomsnittsverdiene fra fototransistoren.
*/

// Millis() og tidspunkt siden forrige sensorsjekk brukes til aa sjekke om LED-status skal ha mulighet til aa endres.
  if ((millis()-(forrigeSensorSjekkTidspunkt)) > millisMellomHverSensorSjekk) {
    forrigeSensorSjekkTidspunkt = millis();

// Under foelger if-sjekker for forskjellige lysforhold.

    // Dersom sensorgjennomsnittet er under roed grenseverdi inkludert kalibrering, men samtidig hoeyere enn groenn grenseverdi (det moerkeste)
    // saa betyr det at det er plassert en roed, "negativ" brikke.
    // Det kan derimot ogsaa vaere at denne endringen i lys er fordi en groenn "positiv" brikke er i ferd med aa plasseres.
    // Derfor har vi lagt til en liten delay, roedtLysDelay, som oeker frem til statusen saa endres.
    if((hentSensorGjennomsnitt() < (roedGrenseVerdi + (sensorHoyesteReading/kalibreringsMultiplier))) && ((hentSensorGjennomsnitt()) >= groennGrenseVerdi)){
      roedtLysDelay += 1;
        if(roedtLysDelay > 5) { // Merk: Denne er en multippel relativ til millisMellomHverSensorSjekk og er ikke tidsjustert!
          sensorLysStatus = 1;
          erSisteBrikkePositivStatus = false;
          erSisteBrikkeNegativStatus = true;
          utskrift("Roed brikke funnet.");
        }
    }

    // Hvis sensorgjennomsnittet er helt moerkt vil det si at brikken er positiv, eller groenn. Koden under endrer saa brettets status til groennt.
    if(hentSensorGjennomsnitt() < (groennGrenseVerdi)) {
      roedtLysDelay = 0;
        sensorLysStatus = 0;
        erSisteBrikkePositivStatus = true;
        erSisteBrikkeNegativStatus = false;
        utskrift("Groenn brikke funnet.");
    }

    // Dersom sensoren er hoeyere enn den roede grensen inkludert kalibrering, saa er ingen brikke plassert. Lysstatusen settes da til 2, som skrur av alle lysene.
    if(hentSensorGjennomsnitt() >= (roedGrenseVerdi + (sensorHoyesteReading/kalibreringsMultiplier))){
      roedtLysDelay = 0;
      utskrift("Ingen brikke plassert.");
      sensorLysStatus = 2;
      }
    }


// Delen under spiller av animasjoner basert paa hvilken status brettet er i.
// If-sjekken kjoerer hver gang det har gaatt en viss tidsperiode mellom hver animasjon.
  if ((millis()-forrigeAnimasjonsTidspunkt) > millisMellomHverAnimasjon) {
    // If-sjekk for status
    if (sensorLysStatus == 0){
      // Dersom animasjonen for status 0, positiv, ikke har blitt igangsatt, spilles den av.
      if (erPositivAnimasjonAktiv == false){
        nullstillNegativAnimasjon();               // Skrur blant annet av negative lys
        spillPositivAnimasjon();
      }
      else{
        nestePositivAnimasjon();                   // Kaller paa neste frame hvis positiv animasjon aktiv allerede er true
      }
    }

    // Tilsvarende status for 1, negativ brikke.
    if (sensorLysStatus == 1){
      if (erSisteBrikkeNegativStatus == true){
        if (erNegativAnimasjonAktiv == false){
          nullstillPositivAnimasjon();
          spillNegativAnimasjon();
        }
        else{
          nesteNegativAnimasjon();                // Neste frame hvis negativ animasjon aktiv er true
        }
      }
    }

    // Ingen brikke plassert
    if (sensorLysStatus == 2){
      // Animasjoner nullstilles.
      nullstillPositivAnimasjon();
      nullstillNegativAnimasjon();
      }
   }
}

// Programloop slutter her, loopes paa repeat per maskin-tick.


// Metoder
// Metode for aa lese fototransistor-sensor, ta vare paa verdiene og etter hvert regne ut en gjennomsnittsverdi.
void lesSensor() {
  if (avlesningNummer >= (sensorAvlesningerFoerGjennomsnitt)){      // Oensket presisjon paa sensoren kan justeres med sensorAvlesningerFoerGjennomsnitt.
    avlesningNummer = 0;
  }
  sensorArray[avlesningNummer] = analogRead(photoTran);             // Sensoren leses av
  avlesningNummer += 1;
}

// Utregning av gjennomsnitt og returnerer verdien.
float hentSensorGjennomsnitt() {
  midlertidigSum = 0;
  for (byte i = 0; i < sensorAvlesningerFoerGjennomsnitt; i = i + 1) {
    midlertidigSum += sensorArray[i];
  }
  midlertidigSum = (midlertidigSum / sensorAvlesningerFoerGjennomsnitt);
  Serial.println(midlertidigSum);
  return midlertidigSum;
}

// Metoder for aa justere lys
// Skrur av alle lys
void settAlleLysTilAv(){
  for (int i = 0; i < sizeof(gronnLed); i++){
    settGrontLysNummerXTilAv(i);
    }
  for (int i = 0; i < sizeof(rodLed); i++){
    settRodtLysNummerXTilAv(i);
    }
}

// Skrur paa alle lys
void settAlleLysTilPaa(){
  for (int i = 0; i < sizeof(gronnLed); i++){
    settGrontLysNummerXTilPaa(i);
    }
  for (int i = 0; i < sizeof(rodLed); i++){
    settRodtLysNummerXTilPaa(i);
    }
}

// Gront lys paa og av
void settGrontLysNummerXTilPaa(int x){
  digitalWrite(gronnLed[x], HIGH);
}

void settGrontLysNummerXTilAv(int x){
  digitalWrite(gronnLed[x], LOW);
}

// Rodt lys paa og av
void settRodtLysNummerXTilPaa(int x){
  digitalWrite(rodLed[x], HIGH);
}

void settRodtLysNummerXTilAv(int x){
  digitalWrite(rodLed[x], LOW);
}

// Avspilling av positiv lys-animasjon
void spillPositivAnimasjon(){
  utskrift("Positiv animasjon spiller");
  erPositivAnimasjonAktiv = true;
}

// Neste animasjon - brukes naar animasjonen allerede er aktiv.
void nestePositivAnimasjon(){
  if (positivAnimasjonsFrame < positivAnimasjonMaxFrameLengde) {
    spillPositivAnimasjonFrame(positivAnimasjonsFrame);
    positivAnimasjonsFrame += 1;
  }
  else if (positivAnimasjonsFrame >= positivAnimasjonMaxFrameLengde) {
    loopPositivAnimasjon();
  }
}

// Avspilling av negativ animasjon
void spillNegativAnimasjon(){
  utskrift("Negativ animasjon spiller");
  // negativAnimasjonsFrame = 0;
  erNegativAnimasjonAktiv = true;
}

// Neste animasjon - brukes naar animasjonen allerede er aktiv.
void nesteNegativAnimasjon(){
  if (negativAnimasjonsFrame < negativAnimasjonMaxFrameLengde) {
    spillNegativAnimasjonFrame(negativAnimasjonsFrame);
    negativAnimasjonsFrame += 1;
  }
  else if (negativAnimasjonsFrame >= negativAnimasjonMaxFrameLengde) {
    loopNegativAnimasjon();
  }
}

// Setter positiv animasjonsframe til 0, brukes i animasjonsloop
void loopPositivAnimasjon(){
  positivAnimasjonsFrame = 0;
}

// Setter negativ animasjonsframe til 0, brukes i animasjonsloop
void loopNegativAnimasjon(){
  negativAnimasjonsFrame = 0;
}

// Skrur av groenne lys og setter animasjonsstatus til false
void nullstillPositivAnimasjon(){
  for (int i = 0; i < positivAnimasjonMaxFrameLengde; i++){
    settGrontLysNummerXTilAv(i); }
  erPositivAnimasjonAktiv = false;
  positivAnimasjonsFrame = 0;
}

// Skrur av rode lys og setter animasjonsstatus til false
void nullstillNegativAnimasjon(){
  for (int i = 0; i < negativAnimasjonMaxFrameLengde; i++){
    settRodtLysNummerXTilAv(i); }
  erNegativAnimasjonAktiv = false;
  negativAnimasjonsFrame = 0;
}


// Lys-animasjoner:
// Positiv animasjon (gront lys)
void spillPositivAnimasjonFrame(int frameInndata) {
  utskrift("spillPositivAnimasjonFrame kalt paa");
  // Animasjontidspunkt oppdateres
  forrigeAnimasjonsTidspunkt = millis();
  // frameInndata brukes til aa spille av forskjellige animasjoner.
  if (frameInndata == 0) { settGrontLysNummerXTilPaa(0); settGrontLysNummerXTilPaa(1); settGrontLysNummerXTilPaa(2); settGrontLysNummerXTilPaa(3); }
  if (frameInndata == 1) { settGrontLysNummerXTilAv(0); settGrontLysNummerXTilAv(1); settGrontLysNummerXTilPaa(2); settGrontLysNummerXTilPaa(3); }
  if (frameInndata == 2) { settGrontLysNummerXTilPaa(0); settGrontLysNummerXTilPaa(1); settGrontLysNummerXTilAv(2); settGrontLysNummerXTilAv(3); }
  if (frameInndata == 3) { settGrontLysNummerXTilAv(0); settGrontLysNummerXTilPaa(1); settGrontLysNummerXTilPaa(2); settGrontLysNummerXTilAv(3); }
}

// Negativ animasjon (rodt lys)
void spillNegativAnimasjonFrame(int frameInndata) {
  utskrift("spillNegativAnimasjonFrame kalt paa");
  forrigeAnimasjonsTidspunkt = millis();
  if (frameInndata == 0) { settRodtLysNummerXTilPaa(0); settRodtLysNummerXTilPaa(1); settRodtLysNummerXTilPaa(2); settRodtLysNummerXTilPaa(3); }
  if (frameInndata == 1) { settRodtLysNummerXTilPaa(0); settRodtLysNummerXTilAv(1); settRodtLysNummerXTilPaa(2); settRodtLysNummerXTilPaa(3); }
  if (frameInndata == 2) { settRodtLysNummerXTilPaa(0); settRodtLysNummerXTilPaa(1); settRodtLysNummerXTilAv(2); settRodtLysNummerXTilPaa(3); }
  if (frameInndata == 3) { settRodtLysNummerXTilPaa(0); settRodtLysNummerXTilPaa(1); settRodtLysNummerXTilPaa(2); settRodtLysNummerXTilAv(3); }
}

// Kalibrering
void kalibrer() {
forrigeKalibreringsTidspunkt = millis();    // Oppdaterer timestamp for naar kalibrering sist ble utfoert.

  // If-sjekk for oppdatering av maksverdi.
    sensorReading = analogRead(photoTran);
    if (sensorReading > sensorHoyesteReading) {

      // Oppdatering av maksverdi
      sensorHoyesteReading = sensorReading;

      // Prints ved ny maksverdi
      utskrift("Ny maksverdi funnet og oppdatert: ");
      Serial.println(sensorHoyesteReading);
    }

    // If-sjekk for oppdatering av minimumsverdi
      if (sensorReading < sensorMinsteReading) {

        // Oppdatering av minimumsverdi
        sensorMinsteReading = sensorReading;

        // Prints ved ny minimumsverdi
        utskrift("Ny minimumsverdi funnet og oppdatert: ");
        Serial.println(sensorMinsteReading);
      }

      // Automatisk justering/innskrenkning av kalibreringsnivaaer over tid.
      sensorHoyesteReading -= 2;
      sensorMinsteReading += 1;
    }
