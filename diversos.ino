
#define   LEDPIN          LED_BUILTIN     // pino do LED de status
#define   TERRA                     3     // pino usado como GND para o "shield" do PIMA
/*
#define   RLY1_LIGA_CUR            11     // Pino do RELÉ que fecha o circuito de corrente do sistema elétrico da jiga
#define   RLY2_CTRL_CUR            11     // controle de corrente - mudou de 13 para 11   - ver 2.1.2: deixou de ser usado
#define   BIMANUAL                 31     // Acionamento bimanual do operador
#define   VALVULA1                 32     // Valvula de acionamento do cilindro que desce o medidor nas agulhas de corrente/tensao
#define   CHAVE_1                 A10     // chave de fim de curso do berço dos medidores na posição de ensaio
#define   RLY_AC                   34     // liga a alimentação AC nos medidores
#define   RLY_IMA                  35     // acionamento do eletroima de segurança da movimentação do berço dos medidores
*/
#define   ESTADO_DESLIG           LOW     // Estado do Rele em NF
#define   ESTADO_LIGADO          HIGH     // Estado do Rele em NA
#define   CORRENTE_LO             LOW     // Selecionado Corrente Baixa no relé de controle
#define   CORRENTE_HI            HIGH     // Selecionado Corrente Alta no relé de controle
#define   SSR_LIGADO             HIGH     // Nivel de Sinal para acionamento do Relé de Estado Solido
#define   SSR_DESLIG              LOW     // Nivel de Sinal para desligamento do Relé de Estado Sólido
#define   DELAY_CORRENTE          800     // aguarda "x" milissegundos antes de verificar a corrente maxima
#define   LIMITE_CORRENTE         560     // valor maximo permitido para a corrente
#define   CORRENTE_BAIXA          230     // valor que se estiver menor que isso devemos subir a corrente (depois de estabilizar)

/*    Valores de Corrente medidos de referencia durante testes
 * 
 *    INOX - LO  106
 *    INOX - HI  306
 *     AÇO - LO  438
 *     AÇO - HI  739
 */


int fLed = 150;                        // Variável da frequencia de piscada do led onboard
bool lendoCorrente = false;            // flag de controle da rotina de leitura ciclica do AD
const byte adcPin = 0;                 // equivale ao port A0 na placa do Arduino
int maximo = 0;                        // valor maximo lido no sensor de corrente (Ipico da senoide)
uint32_t estabilizar = 0;              // tempo aguardando estabilizar a corrent antes de decidir trocar de corrente
bool regular_corrente = false;         // solicita verificar a corrente
bool corrente_alta = false;            // "alarme" de corrente alta

void diversosInit() {  // inicialização de rotinas diversas auxiliares
  pinMode(LEDPIN, OUTPUT);             // Pino do LED de Status
  pinMode(TERRA, OUTPUT);              // define o pino de terra como saída
  
  digitalWrite(VALVULA,LOW);
  digitalWrite(IMA,LOW);
  digitalWrite(RELE_AC,LOW);
  digitalWrite(RLY2_CTRL_CUR,LOW);
  digitalWrite(RLY1_LIGA_CUR,LOW);

  digitalWrite(TERRA, LOW);            // coloca o pino em 0
}


// ###################  PISCA-PISCA do LED de STATUS  ###################
unsigned long ledblink = tmrnow;     // variavel de contagem do tempo de piscada do LED
boolean ledstate = false;              // variavel de estado do LED
void ledEventLoop() {                  // Rotina para piscar o led onboard... sem bloqueio de execução
    if (tmrnow - ledblink > fLed) {  // se o tempo atual menos o tempo inicial é maior que o tempo entre troca de estado...
      ledblink = tmrnow;             // reseta o contador de tempo
      digitalWrite(LEDPIN, ledstate);  // altera o estado do pino do LED
      ledstate = (!ledstate);          // inverte flag de estado do pino do LED
    }
}

void controleCorrenteInit(void) {
  // inicialização do AD e dos pinos de controle da corrente
  ADCSRA  =  bit (ADEN);   // turn ADC on
  ADCSRA |=  bit (ADPS0) |  bit (ADPS1) | bit (ADPS2);  // Prescaler of 128
  ADMUX   =  bit (REFS0) | (adcPin & 0x07);  // AVcc 
  
  pinMode(RLY1_LIGA_CUR, OUTPUT);  // Controle da corrente (0 - Alta   1 - Baixa)
  pinMode(RLY2_CTRL_CUR, OUTPUT);  // Liga/Desliga corrente

  digitalWrite(RLY2_CTRL_CUR, CORRENTE_LO);  // Corrente Baixa
  digitalWrite(RLY1_LIGA_CUR, ESTADO_DESLIG);  // Mantem a Corrente Desligada

  lendoCorrente = false;
}

void ctrlCorrEventLoop(void) {
  if (!lendoCorrente) {
    bitSet (ADCSRA, ADSC);  // start a conversion
    lendoCorrente = true;
    }
    
  // the ADC clears the bit when done
  if (bit_is_clear(ADCSRA, ADSC)) {
    int value = ADC;  // read result
    lendoCorrente = false;
    if (value > maximo) {
      maximo = value;
      //Serial.println(maximo); 
    }
  }
  if (!corrente_alta && maximo > LIMITE_CORRENTE) {  // mudamos a corrente para HI, porem depois de um tempo subiu demais.. 
    corrente_alta = true;
    digitalWrite(RLY2_CTRL_CUR, CORRENTE_LO);  // Corrente Baixa
  } else if (regular_corrente) {
    if (tmrnow - estabilizar > DELAY_CORRENTE) {
      // passou o tempo de estabilização... verificar a condição e desligar timer
      regular_corrente = false;  // só vem aqui uma vez
      if (maximo < CORRENTE_BAIXA) {
        digitalWrite(RLY2_CTRL_CUR, CORRENTE_HI);  // Corrente Alta
      }
    }
  }
}

void ligaCorrente(void) {
  // Rotina que aciona a corrente e aciona sistema que regula nivel de corrente conforme parafuso
  regular_corrente = true;
  corrente_alta = false;
  estabilizar = tmrnow;

              //Aciona cilindro
  digitalWrite(RLY2_CTRL_CUR, CORRENTE_LO);  // Corrente Baixa
  digitalWrite(RLY1_LIGA_CUR, ESTADO_LIGADO);  // Ligar a Corrente
}


void desligaCorrente(void) {
  // Rotina que desliga corrente nos medidores
  regular_corrente = false;
  digitalWrite(VALVULA,LOW); //Desliga cilindro
 digitalWrite(RELE_AC,LOW); //Desliga cilindro
 digitalWrite(IMA,LOW); //Desliga cilindro
 
  if(digitalRead(START_OP) == 1)
    {
    for(int n = 0;n < 6; n++)
             ledSet(n,8);
   }

  digitalWrite(RLY1_LIGA_CUR, ESTADO_DESLIG);  // Corrente Desligada
}
