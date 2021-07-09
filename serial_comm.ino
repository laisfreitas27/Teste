/*
 *   SERIAL COMPUTADOR
 *   
 *   Rotina de comunicação e transmisão dos pacotes recebidos pelos medidores ao computador
 *   
 *   
 *   Protocolo proprio
 *   
 *   Velocidade: 115.200 bps
 *   
 *   
 *   STATUS: Em teste
 *   
 */

 #define      SERIAL_TX_BUFFER_SIZE          128      // ajusta o buffer serial de transmissão para o máximo recomendado
 #define                       BAUD        19200       // velocidade da serial de retransmissão dos pacotes PIMA ao computador

String inputString = "";               // Buffer Serial de Entrada
volatile boolean comandoSerial = false;  // Global do flag de palavra serial recebida



void serialInit() {         // rotina de inicialização da serial de retransmissão dos pacores recebidos ao medidor
  Serial.begin(BAUD);
  while (!Serial) {  ; }    // wait for serial port to connect. Needed for native USB port only
  //if (DEBUG) Serial.println(F("Serial do Computador inicializado em modo DEBUG! ")); 
  inputString.reserve(64);
  comandoSerial = false;
  if (DEBUG) Serial.println(F("Serial de Comunicacao inicializado em modo DEBUG"));
}

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();      // get the new byte:
    inputString += inChar;                  // add it to the inputString:
    if (inChar == '\n') {                   // if the incoming character is a newline, set a flag so the main loop can
      comandoSerial = true;                 // do something about it:
    }
  }
}

void serialEventLoop() {                    // rotina de retransmissão dos pacotes recebidos via serial, identificando o endereço do medidor
  if (comandoSerial) { 
    comandoSerial = false;                  // reinicia o flag já que vamos tratar o comando atual
    String str = inputString;               // salva string recebida em string temporária
    inputString = "";                       // reinicializa o buffer de recepção serial
    //Serial.print(F("Recebi o Comando: "));
    //Serial.print(str);
    if (str.length() > 1) {                 // verifica se a string tem o tamanho minimo aceitavel (pelo menos 2 caracteres)
      if (str.substring(0,1) == "l") {      // compara para saber se é este comando: teste do LED
        // executa o comando
        String conv = str.substring(1,2);   // qual LED
        int tm1 = (conv.toInt()) - 1;
        conv = str.substring(2,3);
        int tm2 = conv.toInt();             // cor do LED
        ledSet(tm1, tm2);                   // ajusta a cor conforme solicitado
      } else if (str.substring(0,1) == "N") {      // compara para saber se é este comando: Status da placa de Rede
        // executa o comando
        //Serial.println(str);
        String conv = str.substring(1,2);   // qual Status
        int tm1 = (conv.toInt());
        if (tm1 == 0) {
          bloqueiaEnsaio = false;
        } else if (tm1 == 1) {
          bloqueiaEnsaio = true;
          lcdTextbox(F("      ERRO!!!    "), 2);
          lcdTextbox(F("Sem Placa de Rede"), 3);
          lcdTextbox(F("Verificar o Cabo "), 4);
          lcdTextbox(F("e resetar a jiga "), 5);
        }
      } 
    }
  }
}
