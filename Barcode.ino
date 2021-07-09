#include <usbhid.h>
#include <usbhub.h>
#include <hiduniversal.h>
#include <hidboot.h>
#include <SPI.h>


volatile byte keybuff[100];  // buffer de caracteres recebidos
volatile uint8_t tmkb=0;  // quantidade de caracteres recebidos na ultima leitura
volatile bool novoBarcode = false;  // indica que o leitor completou uma leitura (recebeu um ENTER)
byte idDoMedidor[10];  // buffer que carrega a ID do medidor. SEMPRE tem 10 caracteres NUMERICOS.
byte idTampaComp[16];  // ID lida na tampa do medidor, sem formatacao. No banco de dados este campo tem no maximo 15 bytes.
int  idTampaCompTm;  // Tamanho do ID completo armazenado
bool novaID = false;  // indica que foi recebido e formatado corretamente uma nova ID de Medidor, para a rotina de ensaio.
void lcdTextbox(String msg, int boxn, bool scrool=false);
bool iniciarEnsaio = false;  // flag de (re)inicio do ensaio
bool rodarEnsaio = false;  // flag para indicar que já pode iniciar o ensaio (todos os códigos já foram lidos)
bool podeEnviar = false;
int numTampa = 0;  // indicador de qual codigo de barras estamos recebendo
byte tampa[6][10];  // matriz de dados de todas as tampas dos medidores
byte tampaCompleta[6][16];  // matriz de dados de todas as tampas completas dos medidores
int tampaCompletaTm[6];  // matriz de dados de todas as tampas completas dos medidores
bool sempima = false;  // flag que indica se vamos rodar o ensaio com somente pima
bool semhorus = false;  // flag que indica se vamos rodar ser rastreabilidade   <<-- default True desabilita o teste
bool cancelar = false;  // flag de quando o ensaio foi cancelado. Conforme o caso informamos no Horus.
uint32_t cicloInicio = 0;  // registro do tempo de inicio do ensaio
bool bloqueiaEnsaio = false;  // Bloqueia o ensaio caso a placa de rede esteja inoperante

bool pin12 = false;
bool pin11 = false;
bool pin30 = false;


// ### Funcoes de Outros Arquivos - BUG do Arduino IDE ###
void ledSet(uint8_t nled, uint8_t tmp);


class KbdRptParser : public KeyboardReportParser
{
    void PrintKey(uint8_t mod, uint8_t key);

  protected:
    void OnControlKeysChanged(uint8_t before, uint8_t after);

    void OnKeyDown  (uint8_t mod, uint8_t key);
    void OnKeyUp  (uint8_t mod, uint8_t key);
    void OnKeyPressed(uint8_t key);
};

void KbdRptParser::PrintKey(uint8_t m, uint8_t key)
{
  MODIFIERKEYS mod;
  *((uint8_t*)&mod) = m;
  //Serial.print((mod.bmLeftCtrl   == 1) ? "C" : " ");
  //Serial.print((mod.bmLeftShift  == 1) ? "S" : " ");
  //Serial.print((mod.bmLeftAlt    == 1) ? "A" : " ");
  //Serial.print((mod.bmLeftGUI    == 1) ? "G" : " ");

  //Serial.print(" > ");
  PrintHex<uint8_t>(key, 0x80);
  //Serial.print("< ");

  //Serial.print((mod.bmRightCtrl   == 1) ? "C" : " ");
  //Serial.print((mod.bmRightShift  == 1) ? "S" : " ");
  //Serial.print((mod.bmRightAlt    == 1) ? "A" : " ");
  //Serial.println((mod.bmRightGUI    == 1) ? "G" : " ");
};

void KbdRptParser::OnKeyDown(uint8_t mod, uint8_t key)
{
  //Serial.print("DN ");
  //PrintKey(mod, key);
  uint8_t c = OemToAscii(mod, key);

  if (c)
    OnKeyPressed(c);
}

void KbdRptParser::OnControlKeysChanged(uint8_t before, uint8_t after) {
  // Esta função (desabilitada por hora) serve para decodificar as teclas modificadoras, como CTRL e ALT.
  
  /*
  MODIFIERKEYS beforeMod;
  *((uint8_t*)&beforeMod) = before;

  MODIFIERKEYS afterMod;
  *((uint8_t*)&afterMod) = after;

  if (beforeMod.bmLeftCtrl != afterMod.bmLeftCtrl) {
    Serial.println("LeftCtrl changed");
  }
  if (beforeMod.bmLeftShift != afterMod.bmLeftShift) {
    Serial.println("LeftShift changed");
  }
  if (beforeMod.bmLeftAlt != afterMod.bmLeftAlt) {
    Serial.println("LeftAlt changed");
  }
  if (beforeMod.bmLeftGUI != afterMod.bmLeftGUI) {
    Serial.println("LeftGUI changed");
  }

  if (beforeMod.bmRightCtrl != afterMod.bmRightCtrl) {
    Serial.println("RightCtrl changed");
  }
  if (beforeMod.bmRightShift != afterMod.bmRightShift) {
    Serial.println("RightShift changed");
  }
  if (beforeMod.bmRightAlt != afterMod.bmRightAlt) {
    Serial.println("RightAlt changed");
  }
  if (beforeMod.bmRightGUI != afterMod.bmRightGUI) {
    Serial.println("RightGUI changed");
  }
  */
}

void KbdRptParser::OnKeyUp(uint8_t mod, uint8_t key)
{
  //Serial.print("UP ");
  //PrintKey(mod, key);
}

void KbdRptParser::OnKeyPressed(uint8_t key)
{
  // Esta será a função que usaremos para armazenar o buffer e liberar quando chegar um "enter"
  // Apenas para ler e disponibilizar, não será processada a informação
  //Serial.print(key, HEX);
  //Serial.print(" ASCII: ");
  //Serial.println((char)key);
  if (key == 0x13 || key == 0x0D) { // Porque alguem tinha tirado o 0x13 daqui????
  //if (key == 0x0D) {
  //  String c;
    // int i = c.length();
    // aqui detectamos um enter
    novoBarcode = true; 
    if (DEBUG) Serial.print(F("Status do novoBarcode: "));
    if (DEBUG) Serial.println(novoBarcode);
     //keybuff[tmkb-1]= '\0';  
  } else {
    // qualquer coisa diferente de enter colocamos no buffer
    keybuff[tmkb++] = key;
    
    if (DEBUG) Serial.print(key);
  }
};

USB     Usb;
USBHub     Hub(&Usb);
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    HidKeyboard(&Usb);

KbdRptParser Prs;


void barcodeInit(){
  // Inicialização da placa USB e da leitora de código de barras
  if (Usb.Init() == -1) {
    //Serial.println("ERRO na placa USB!");
    //Serial.println(Usb.Init());
  }
  delay(200);
  HidKeyboard.SetReportParser(0, &Prs);
  //Serial.println(Usb.Init());
  //Serial.println("Passou!!"); 
  bloqueiaEnsaio = false;
}

void barcodeEventLoop() { 
  // Atualização da leitura do código de barras
  Usb.Task();   // rotina de eventos da placa USB
  if (novoBarcode) {
    barcodeRecebido();
  }
}

void barcodeRecebido(void) {
  // Rotina que trata nova entrada de código de barras.
  // Decodifica comandos e formata dados da ID.
  String palavra;
  for (int n=0;n<tmkb;n++) {
    palavra += ((char)keybuff[n]);
  }
  if (DEBUG) Serial.println(palavra);
  if (palavra == "INICIAR" && !bloqueiaEnsaio) {
    // comando para iniciar/reiniciar o ensaio
    if (DEBUG) Serial.println(F("Recebi um comando para (re)iniciar o Ensaio via codigo de Barras!"));
  //Mit
  cancelar = false;
  for(int n=0;n <6;n++)
         ledSet(n, 1);
 if (digitalRead(START_OP) == 0)
 {   

    cancelar = false;
    iniciarEnsaio = true;
    sempima = false;   // com pima
    semhorus = false;  // com horus

    unsigned long nwm = 0;
    digitalWrite(IMA,HIGH);  
 
  int tmp = 1000;
    nwm = millis();
    while(millis() < nwm  + tmp)
           wdt_reset();
    digitalWrite(VALVULA,HIGH);  
 
     tmp = 2000; nwm=0;
    nwm = millis();
    while(millis() < nwm  + tmp)
           wdt_reset();

   digitalWrite(RELE_AC,HIGH); 

  //Fim Mit
    
    cicloInicio = tmrnow;  // grava o inicio do ciclo
 } 
  } else if (palavra == "SEMPIMA" && !bloqueiaEnsaio) {
    // vamos apenas ligar a corrente sem testar o PIMA
    // enviar mensagem na tela da Jiga
    // logar no Horus...
    // vamos apenas ligar a corrente sem testar o PIMA
    // enviar mensagem na tela da Jiga
    // logar no Horus...
 
  //  cancelar = false;
   // iniciarEnsaio = true;
    //sempima = true;  // sem pima
   // semhorus = false;  // com horus
 //  cicloInicio = tmrnow;  // grava o inicio do ciclo
 //Mit
 for(int n=0;n <6;n++)
         ledSet(n, 1);
 if (digitalRead(START_OP) == 0)
 {   
    cancelar = false;
    iniciarEnsaio = true;

      unsigned long nwm = 0;
    digitalWrite(IMA,HIGH);  
  int tmp = 1000;
    nwm = millis();
    while(millis() < nwm  + tmp)
           wdt_reset();
    digitalWrite(VALVULA,HIGH);  
 
     tmp = 2000; nwm=0;
    nwm = millis();
    while(millis() < nwm  + tmp)
           wdt_reset();

   digitalWrite(RELE_AC,HIGH); 

    
    sempima = true;  // sem pima
    semhorus = false;  // com horus
    cicloInicio = tmrnow;  // grava o inicio do ciclo
 }
 
  } else if (palavra == "SOMENTELIGAR") {
    // vamos apenas ligar a corrente sem testar o PIMA
    cancelar = false;
    iniciarEnsaio = true;
    sempima = true;  // sem pima
    semhorus = true;  // sem horus
  } else if (palavra == "CANCELAR") {
    // vamos desligar a corrente
    // cancelar ensaio em andamento? é possivel?
    // enviar mensagem na tela da Jiga
    cancelar = true;
  } else if (palavra == "SEMREDE") {
    // Cancela o bloqueio do ensaio quando sem placa de rede
    bloqueiaEnsaio = false;
  } else if (!bloqueiaEnsaio){
    // se não for algum comando, é código de barras de medidor... enviar este código para rotina de testes
    // só entra aqui caso a placa de rede esteja funcionando, ou se manualmente foi sobrescrito esta funcao
    salvarId();  // antes de formatar a ID Energisa, vamos salvar o barcode lido para LOG no sistema Horus
    formatarID();  // formatar a nova ID antes de efetuar reset do buffer
    if (podeEnviar) {
      enviarID();
    }
    //* DEBUG
    if (DEBUG) {
      Serial.print("Valor Lido pelo Scanner: ");
      Serial.println(palavra);
      Serial.print("           ID Formatada: ");
      for (int n=0;n<10;n++) {
        Serial.print((char)idDoMedidor[n]);
      }
      Serial.println();
    }//*/
  }
  
  tmkb = 0;  // reset do buffer do teclado
  novoBarcode = false;
  
}

void formatarID(void) {
  // começamos identificando se esta ID é o caso especial do cliente Energisa
  if ((tmkb == 11) && (keybuff[0] == 'D')) {
    // Este é o caso especial Energisa
    idDoMedidor[0] = '0';
    idDoMedidor[1] = '0';
    idDoMedidor[2] = '0';
    for (int n=2;n<9;n++) {
      idDoMedidor[n+1] = keybuff[n];
    }
    return;
  } else if ((tmkb == 11)) {
    // Este é o caso especial do cliente EQUATORIAL
    for (int n=0;n<10;n++) {
      idDoMedidor[n] = keybuff[n];
    }
    return;
  } else if ((tmkb == 9) && (keybuff[0] == 'M')) {
    // Este é o caso especial do cliente ELEKTRO
    idDoMedidor[0] = '1';
    idDoMedidor[1] = '0';
    idDoMedidor[2] = '1';
    //idDoMedidor[3] = '1';  // este item pode ser 2, ou 3.. pegar o que foi lido mesmo
    for (int n=1;n<8;n++) {  // corrigir esta parte conforme alteracao acima
      idDoMedidor[n+2] = keybuff[n];
    }
    return;
  } else {
    // Esta é a formatação habitual da ID do medidor
    if (tmkb > 9) {
      // descarta os caracteres sobrando, pegando os 10 ULTIMOS caracteres
      int m=0;
      for (int n=(tmkb-10);n<tmkb;n++) {
        idDoMedidor[m++] = keybuff[n];
      }
    } else {
      // completa os espaços que faltam para 10 com zeros, à esquerda
      int falta = 10 - tmkb;
      for (int n=0;n<falta;n++) {
        idDoMedidor[n] = '0';
      }
      for (int n=0;n<tmkb;n++) {
        idDoMedidor[n+falta] = keybuff[n];
      }
    }
    // vamos remover qualquer letra que possa ter no código substituindo por zero
    for (int n=0;n<10;n++) {
      if ((idDoMedidor[n] < 0x30) || (idDoMedidor[n] > 0x39)) {
        idDoMedidor[n] = 0x30;
      }
    }
  }
}

void salvarId(void){
  // rotina que salva a ID, lida sem modificacoes, em buffer especifico
  idTampaCompTm = tmkb;  // salva o tamanho do ID lido na tampa
  for (int n=0;n<idTampaCompTm; n++) {
    idTampaComp[n] = keybuff[n];
  }
}

void enviarID() {
  if (numTampa < 6) {  // verifica se não foi lido mais códigos que o suportado, por qualquer motivo
    // envia o ID recebido para o buffer de ensaio
    tampaCompletaTm[numTampa] = idTampaCompTm;
    for (int n=0;n<idTampaCompTm; n++) {
      tampaCompleta[numTampa][n] = idTampaComp[n];
    }
    String tmpconv;
    for (int n=0;n<10;n++) {
      tampa[numTampa][n] = idDoMedidor[n];
      tmpconv += (char)idDoMedidor[n];
    }
    tmpconv += "        ";
    // incrementa contador do buffer de codigo de barras
    numTampa += 1;
    // atualiza o valor lido no LCD
    lcdTextbox(tmpconv, numTampa);
    ledSet(numTampa-1, 5);
    if (numTampa < 6) {
      // ainda tem mais códigos para ler
      lcdTextbox("__________ <--", numTampa+1);
    } else {
      // este foi o ultimo código
      rodarEnsaio = true;
      for (int n=0;n<6;n++) {
        ledSet(n, 6);  // Coloca os LEDs para piscar em Laranja
      }
    }
  }
}
