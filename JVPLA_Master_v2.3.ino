
/*
 *    --- TESTE DO PIMA MULTIPLO ---
 *    
 *  (com Receptor Multiserial Unidirecional Assíncrono via Software Serial RX)
 *  
 *  Autor: Daniel G. Gulart
 *  
 *  
 *  Licença: Uso restrito. (Contém códigos de terceiros com licença MIT, identificados ao longo do programa.)
 *  
 *  V1 - aplicação Python com seriais multiplos usb/ttl (legacy)
 *  V2 - hardware com software embarcado de teste independente de computador
 *  
 *  CONTROLE DE VERSÃO:
 *    
 *    - 30/04/2019 - v 2.0.0  - Emissão inicial do programa, e teste das rotinas de temporização e recepção multiserial
 *    - 10/05/2019 - v 2.0.3  - Totalmente reescrito. Vamos usar interrupção de pino, 3 seriais por software. 
 *    - 16/05/2019 - v 2.0.4  - Implementação das rotinas de Serial por Hardware, implementacao do decodificador PIMA
 *    - 17/05/2019 - v 2.0.5  - Jiga Stand Alone de teste do PIMA. Teste e ajuste das rotinas de Leitura Barcode USB e LCD Grafico.
 *    - 18/05/2019 - v 2.0.6  - Ajustes na rotina de leitura do Scanner de Código de Barras
 *    - 20/05/2019 - v 2.0.7  - Novos ajustes na rotina de Teclado, usando abordagem diferente na biblioteca de controle da placa USB
 *    - 20/05/2019 - v 2.0.8  - Testes e ajustes na Rotina do LCD
 *    - 22/05/2019 - v 2.0.9  - Implementação da rotina de testes do PIMA. Testes finais.
 *    - 31/05/2019 - v 2.0.10 - Adição do controle de habilitação do circuito de corrente dos medidores analisados.
 *    - 24/06/2019 - v 2.0.11 - Atualização da rotina de Hardware Serial, para manter a porta serial aberta apenas quando o ensaio estiver ativo
 *    -   dez/2019 - v 2.0.12 - Implementacao do controle de LEDs em outro microcontrolador, com ajuste das rotinas de comunicacao com LED.
 *    - 23/01/2020 - v 2.1.0  - Implementacao das rotinas de envio de dados de rastreabilidade para o microcontrolador auxiliar
 *    - 12/02/2020 - v 2.1.1  - Mudança total na rotina do Horus (maior no Slave, mas aqui muda a forma de enviar)
 *    - 07/07/2020 - v 2.1.2  - Alterado rotina de decodificacao da ID do cliente ELEKTRO
 *    - 24/08/2020 - v 2.2.0  - Alterado Palavra de Salvamento no Horus (informa ID do equipamento; Incluido decode para ID Equatorial
 *    - 14/05/2021 - v 2.2.1  - Correcao no algoritmo de decodificacao da ID ELEKTRO (para nova jig MIT 2021)
 *    - 17/05/2021 - v 2.2.1  - Correcao no algoritmo de controle de corrente e sistema de segurança (para nova jig MIT 2021)
 *    
 *    Esta versão está sincronizada com o Slave Versao 1.2.0
 *    
 *  RESULTADO: Finalizado e Validado em 24/08/2020
 * 
 *  Desenvolvido para a placa Arduino Mega2560 ATmega2560 16MHz
 * 
 */

#include <avr/wdt.h>                 // Biblioteca AVR do WDT do ATmega2560


#define      DEBUG      false        // ativa mensagens de debug na serial do computador
#define   BLOCANTE       true        // ativa rotinas blocantes de delay para envio pausado de mensagens ao servidor Horus
#define   DLYBLOCK        150        // quantos milissegundos de delay blocante

//START_OP 10

#define   RLY1_LIGA_CUR            12     // Pino do RELÉ que fecha o circuito de corrente do sistema elétrico da jiga
#define   RLY2_CTRL_CUR            31     // controle de corrente - mudou de 13 para 11  


#define   VALVULA                  32    // Aciona a valvula pneumatica
#define   START_OP                 A10     // informa ao processador que o operador acionou o touch switch
#define   RELE_AC         34
#define   IMA             35

      
bool pos = true;

 long interval = 10000;  
  long previousMillis = 0; 
uint32_t  tmrnow = millis();          // Atualiza o tempo atual, para diminuir acesso à função millis() - que congela interrupção

// ##### BUG do Arduino IDE  #####
void diversosInit(void);           // inicialização de rotinas auxiliares
void pimaSwInit(void);             // inicialização da rotina de Pima Multiserial por Software
void pimaHwInit(void);             // inicialização da rotina de Pima Multiserial por Hardware
void serialInit(void);             // inicialização da porta Serial ligado ao USB do computador
void lcdInit(void);                // inicialização do LCD
void barcodeInit(void);            // inicialização do USB Host e do Leitor de Codigo de Barras
void testeInit(void);              // inicialização da rotina de teste do pima
void pimaSwEventLoop(void);        // Rotina de controle dos pacotes PIMA recebidos por Software Serial
void pimaHwEventLoop(void);        // Rotina de controle dos pacotes PIMA recebidos por Hardware Serial
void testeEventLoop(void);         // Rotina de controle do Teste do PIMA
void serialEventLoop(void);        // Rotina de retransmissão das mensagens recebidas pelo PIMA
void lcdEventLoop(void);           // Rotina de atualização do LCD
void barcodeEventLoop(void);       // Rotina de Leitura de Código de barras, quando disponível.
void ledEventLoop(void);           // Rotina de atualização dos LEDs da Jiga
void controleCorrenteInit(void);


void setup() {
  // Rotinas de Inicialização


  pinMode(START_OP,INPUT_PULLUP);
  
  pinMode(VALVULA,OUTPUT);
  pinMode(RLY2_CTRL_CUR,OUTPUT);
  pinMode(RLY1_LIGA_CUR,OUTPUT);
 
  pinMode(RELE_AC,OUTPUT);
  pinMode(IMA,OUTPUT);

  digitalWrite(VALVULA,LOW);
  digitalWrite(RELE_AC,LOW);
  digitalWrite(IMA,LOW);


  digitalWrite(RLY1_LIGA_CUR,LOW);
  digitalWrite(RLY2_CTRL_CUR,LOW);
  
  diversosInit();           // inicialização de rotinas auxiliares
  pimaSwInit();             // inicialização da rotina de Pima Multiserial por Software
  pimaHwInit();             // inicialização da rotina de Pima Multiserial por Hardware
  serialInit();             // inicialização da porta Serial ligado ao USB do computador
  lcdInit();                // inicialização do LCD
  barcodeInit();            // inicialização do USB Host e do Leitor de Codigo de Barras
  testeInit();              // inicialização da rotina de teste do pima
  controleCorrenteInit();   // inicialização da rotina de controle da corrente dos medidores
  wdt_enable(WDTO_4S);      // Configura o WatchDog Timer para 4 segundos. Travamento ou código blocante maior que 4s causa Reset
  // ATENÇÃO!!  A rotina de inicialização do USB tem uma etapa blocante maior que 2s! tem que usar o WDT >= 4s
}

void loop() {
  // Rotinas de atualização periódica
  tmrnow = millis();        // Atualiza o tempo atual
  pimaSwEventLoop();        // Rotina de controle dos pacotes PIMA recebidos por Software Serial
  pimaHwEventLoop();        // Rotina de controle dos pacotes PIMA recebidos por Hardware Serial
  testeEventLoop();         // Rotina de controle do Teste do PIMA
  serialEventLoop();        // Rotina de retransmissão das mensagens recebidas pelo PIMA
  lcdEventLoop();           // Rotina de atualização do LCD
  barcodeEventLoop();       // Rotina de Leitura de Código de barras, quando disponível.
  ledEventLoop();           // Rotina de atualização dos LEDs da Jiga
  //ctrlCorrEventLoop();      // Rotina de atualização do controle de corrente da Jiga   - Atualizacao 2.1.2: desabilitado controle de corrente
  wdt_reset();              // Reinicia o contador do WatchDog Timer
}
