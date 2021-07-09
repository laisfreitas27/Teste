/*
 *    
 *    Rotinas de Recepção de pacotes PIMA via Hardware Serial
 *    
 * 
 */


#define                  PIMA_BAUD         2400      // velocidade de recepção do PIMA

typedef struct decode_pima
{
  bool recebendo;   // indica se tem pima valido detectado (pelo cabeçalho AA 55)
  bool pimaOK;      // indica quando terminou de receber um pacote PIMA
  bool erroCRC;     // indica quando ocorreu um erro de CRC no pacote recebido
  bool ligado;      // para saber se a porta serial atual esta ligada
  uint8_t tmBuff;   // quantidade de bytes no buffer
  byte buff[300];   // buffer do pima sendo decodificado
  byte idmed[5];    // guarda o numero patrimonial do medidor (BCD)
  byte escind[2];   // guarda escopo e indice (do protocolo)
  byte tamanho;     // guarda o tamanho dos dados a serem recebidos (geralmente 3, mas no pima wh é 5)
  byte dados[10];   // dados trazidos no PIMA
  byte crc[2];      // guarda o crc do pacote. na mesma orde que é recebido, com LSB primeiro.
};

volatile decode_pima pimaDecode[3];
bool ligarHwSerial = false;  // flag para rodar rotina que habilita portas seriais
bool pimaRecebido[6];  // vetor de estado de recepção das IDs de medidor via PIMA.
byte pima[6][10];  // matriz de dados de todas os pimas recebidos dos medidores
void bcdtochar(char* retorno, byte* bcd, int tam);  // declaração da função (que está definida em teste_pima) para uso aqui

void pimaHwInit() {  // rotina de inicialização das portas seriais por hardware
  // modificado na versao 2.0.11: as portas seriais ficam desligadas até que seja iniciado o ensaio
  //Serial1.begin(PIMA_BAUD);
  //Serial2.begin(PIMA_BAUD);
  //Serial3.begin(PIMA_BAUD);
  for (int n=0;n<3;n++) {
    pimaDecode[n].pimaOK = false;
    pimaDecode[n].recebendo = false;
    pimaDecode[n].ligado = false;
    pimaDecode[n].tmBuff = 0;
  }
}

void pimaHwEventLoop() {  // rotina de atualização e controle dos pacotes recebidos via hardware serial
  if (ligarHwSerial) {
    ligarHwSerial = false;  // só vem aqui uma vez a cada solicitação
    ligarSeriais();  // chama rotina que aciona as portas seriais do hardware
  }
  if (pimaDecode[0].ligado) {
    if (Serial1.available()) {
      decodePimaHw(&Serial1, 1);
    }
  }
  if (pimaDecode[1].ligado) {
    if (Serial2.available()) {
      decodePimaHw(&Serial2, 2);
    }
  }
  if (pimaDecode[2].ligado) {
    if (Serial3.available()) {
      decodePimaHw(&Serial3, 3);
    }
  }
}

void ligarSeriais(void) {
  // função que aciona as portas seriais do hardware
  Serial1.begin(PIMA_BAUD);
  Serial2.begin(PIMA_BAUD);
  Serial3.begin(PIMA_BAUD);
  for (int n=0;n<3;n++) {
    pimaDecode[n].ligado = true;  // informa que as portas seriais foram acionadas
  }
}

void enviaPimaHw(HardwareSerial *hwSer, int num) {
  uint8_t qtdados = hwSer->available();
  byte tmpbuff[qtdados]; 
  hwSer->readBytes(tmpbuff, qtdados);
  byte tmpadd = (0x30 + num);
  
  if (DEBUG) {
    Serial.print("Pacote Recebido na Serial ");
    Serial.print(num);
    Serial.print(": ");
    Serial.print(tmpadd, HEX);
    for (int m=0;m<qtdados;m++) {
      Serial.print(" ");
      if (tmpbuff[m] < 0x10) {
        Serial.print("0");
      }
      Serial.print(tmpbuff[m], HEX);
    }
    Serial.println();
  } else { // nao precisa mais enviar dados brutos ao slave...
   // Serial.write(tmpadd);
   // Serial.write(tmpbuff, qtdados);
  }
}

void decodePimaHw(HardwareSerial *hwSer, uint8_t pos) {  // decodficação continua dos pacotes PIMA recebidos
  // se chegou aqui é porque tem dado na serial, vamos descobrir quantos são
  // vamos colocar em um buffer temporário
  uint8_t qtdados = hwSer->available();
  byte tmpbuff[qtdados]; 
  hwSer->readBytes(tmpbuff, qtdados);
  // se não estamos recebendo, verifica se o byte é "AA", muda flag e inicia recepção. Salva no buffer
  if (!pimaDecode[pos-1].recebendo && !pimaDecode[pos-1].pimaOK) {
    for (int n=0;n<qtdados;n++) {
      if (tmpbuff[n] == 0xAA) {
        pimaDecode[pos-1].recebendo = true;
        for (int m=n;m<qtdados;m++) {  // salva a parte válida do buffer
          pimaDecode[pos-1].buff[pimaDecode[pos-1].tmBuff] = tmpbuff[m];
          pimaDecode[pos-1].tmBuff += 1;
        }
        break;
      }
    }
  }
  // se estamos recebendo, colocar no buffer e verificar se já temos uma quantidade mínima de bytes para avaliar
  else if (pimaDecode[pos-1].recebendo) {
    for (int n=0;n<qtdados;n++) {
      pimaDecode[pos-1].buff[pimaDecode[pos-1].tmBuff] = tmpbuff[n];
      pimaDecode[pos-1].tmBuff += 1;
    }
    if (pimaDecode[pos-1].tmBuff > 7) {
      // Tem pelo menos 8 bytes no buffer. O oitavo byte é o numero de bytes deste pacote
      pimaDecode[pos-1].tamanho = pimaDecode[pos-1].buff[7];
      if (pimaDecode[pos-1].tmBuff >= (10 + pimaDecode[pos-1].tamanho)) { 
        // pacote completo! vamos decodificar...
        //pimaDecode[pos-1].pimaOK = true;
        pimaDecode[pos-1].recebendo = false;
        pimaDecode[pos-1].tmBuff = 0;
        for (int n=0;n<5;n++) {
          // Pima ID
          pimaDecode[pos-1].idmed[n] = pimaDecode[pos-1].buff[n+2];
        }
        for (int n=0;n<2;n++) {
          // Escopo e Indice
          pimaDecode[pos-1].escind[n] = pimaDecode[pos-1].buff[n+8];
        }
        for (int n=0;n<2;n++) {
          // CRC
          pimaDecode[pos-1].crc[n] = pimaDecode[pos-1].buff[n+8+pimaDecode[pos-1].tamanho];
        }
        // vamos conferir o CRC do pacote
        byte tmpcrccalc[6+pimaDecode[pos-1].tamanho];
        for (int n=0;n<(6+pimaDecode[pos-1].tamanho);n++) {
          tmpcrccalc[n] = pimaDecode[pos-1].buff[n+2];
        }
        uint16_t tmpcrc = crc_16(tmpcrccalc, 6+pimaDecode[pos-1].tamanho);
        uint8_t crclo = tmpcrc;
        uint8_t crchi = tmpcrc >> 8;
        bool tmpcomp = (crclo == pimaDecode[pos-1].buff[8+pimaDecode[pos-1].tamanho]) && (crchi == pimaDecode[pos-1].buff[8+pimaDecode[pos-1].tamanho+1]);  // confere o CRC recebido e o calculado
        pimaDecode[pos-1].erroCRC = !tmpcomp;  // se ocorreu erro de CRC seta a flag correspondente.
        byte tmpadd = (0x30 + pos);
        
        if (true) {  // estou fixando em true, por desconfiar que estouro de buffer de recepção esteja sobrescrevendo este flag...
          // modifiquei novamente para DEBUG para habilitar a comunicação entre mestre e escravo na versão 2.0.12 
          // vamos mandar uma mensagem de debug ao terminal...
          if (DEBUG) Serial.print("Pacote Recebido na Serial ");
          if (DEBUG) Serial.print(pos);
          if (DEBUG) Serial.print(": ");
          if (DEBUG) Serial.print(tmpadd, HEX);
          
          for (int m=0;m<(10 + pimaDecode[pos-1].tamanho);m++) {
            //Serial.print(" ");
            if (pimaDecode[pos-1].buff[m] < 0x10) {
             // Serial.print("0");
            }
            //Serial.print(pimaDecode[pos-1].buff[m], HEX);
          }
          if (!tmpcomp) {
            if (DEBUG) Serial.print(" (ERRO DE CRC neste pacote!");
          } else{
            // Pacote Ok!! vamos enviar para a rotina do teste
            if (!pimaRecebido[pos-1]) {
              // só registra um novo pacote se não tiver recebido nada ainda (não sobrescreve)
              pimaRecebido[pos-1] = true;  // informa que esta posição recebeu
              bcdtochar(pima[pos-1], pimaDecode[pos-1].idmed, 5);  // aqui converte de BCD para ASCII e já deixa no buffer de retorno
              // abaixo verificamos qual serial sera desligada. Nao é muito elegante esta solução, mas é a mais segura sem teste
              // melhoria implementada para revisão 2.0.11
              if (pos == 1) {
                Serial1.end();  // Desliga serial da posição 1
              } else if (pos == 2) {
                Serial2.end();  // Desliga serial da posição 2
              } else if (pos == 3) {
                Serial3.end();  // Desliga serial da posição 3
              }
            }
            
          }
          //Serial.println();
        } else {
          //Serial.write(tmpadd);
          uint8_t tmptmdata = (10 + pimaDecode[pos-1].tamanho);
          byte *tmpdata = pimaDecode[pos-1].buff;
          //Serial.write(tmpdata, tmptmdata);
        } 
      }
    }
  }
}
