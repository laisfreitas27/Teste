/*
 *   MULTISERIAL PIMA
 *   
 *   Rotina para implementação de recebimento de pacotes unidirecionais seriais
 *   
 *      
 *   STATUS: OK!  (funcional em 15/05/2019)
 *   
 *   PIMA 4:
 *     - Timer 3
 *     - INT 0
 *     - Pino 21
 *     - PD0
 *     
 *   PIMA 5:
 *     - Timer 4
 *     - INT 1
 *     - Pino 20
 *     - PD1
 *     
 *   PIMA 6:
 *     - Timer 5
 *     - INT 4
 *     - Pino 2
 *     - PE4
 *   
 *   
 */

//  ######  DEFINES  ######

#define     RXBUF           256    // tamanho de cada buffer de recepção serial do PIMA

#define     P1_PIN           21    // Pino de entrada do PIMA do medidor 4
#define     P2_PIN           20    // Pino de entrada do PIMA do medidor 5
#define     P3_PIN           2    // Pino de entrada do PIMA do medidor 6

#define     TIMER_INIT    0x4E2    // tempo entre 1.5bit (1.5/2400) = 625us (logo após o start bit)
#define     TIMER_CONT    0x341    // tempo entre  1 bit (1/2400)   = 416.67us
#define     TIMER_PACK    0xC05    // tempo maximo de espera por um byte (start bit) no mesmo pacote (20 ms)
#define     SER_IDLE         20    // tempo sem receber bytes, em milissegundos, para considerar um pacote finalizado

#define     MARK           HIGH    // Nivel logico equivalente a "1" na linha serial
#define     SPACE           LOW    // Nivel logico equivalente a "0" na linha serial


//  ######  GLOBAIS  ######

typedef struct pima_dados {
  uint8_t contador_me;  // contador da maquina de estado do decodificador de pima
  bool recebendo;       // flag de byte sendo recebido
  uint8_t ticker;       // contador de pulsos do timer de recepcao serial
  byte dado;            // buffer do byte sendo recebido
  uint8_t bitPos;       // posição do bit que está sendo recebido
  bool finalizado;      // flag que informa existir pelo menos 1 byte recebido no buffer
  bool pacoteOK;        // flag de fim de recebimento de pacote (equivalente a um "silencio" na serial maior que 20ms)
  uint8_t lastTk;       // ticker do ultimo bit (interrupção)
  bool lastBit;         // estado do ultimo bit (interrupção)
  byte buff[RXBUF];     // buffer de recepção
  uint8_t tmBuff;       // quantidade de bytes no buffer
  uint32_t toutpct;     // contador do timeout de recepção do pacote
  byte stpb;            // flag de erro detectado no stop bit
  byte ovf;             // buffer overflow!!!
};

typedef struct pima_me {       // Struct para guardar lista de etapas do PIMA
  void (*vetor_me)(uint8_t);   // vetor de tratamento da etapa do PIMA
};

typedef struct decode_pima_sw_t {
  byte idmed[5];    // guarda o numero patrimonial do medidor (BCD)
  byte escind[2];   // guarda escopo e indice (do protocolo)
  byte tamanho;     // guarda o tamanho dos dados a serem recebidos (geralmente 3, mas no pima wh é 5)
  byte dados[10];   // dados trazidos no PIMA
  byte crc[2];      // guarda o crc do pacote. na mesma orde que é recebido, com LSB primeiro.
} decode_pima_t;

volatile decode_pima_t decodePima;

volatile pima_dados dadosPima[3];



void pimaSwInit() {
  initTimers();
  for (int n=0;n<3;n++) {
    dadosPima[n].tmBuff = 0;  // zera o contador de tamanho/posição do buffer
    memset(dadosPima[n].buff, 0x00, RXBUF);  // limpa todo o buffer de recepção
    dadosPima[n].recebendo = false;  // zera flag de recepção
    dadosPima[n].pacoteOK = false;  // zera a flag de pacote finalizado
    dadosPima[n].dado = 0x00;
    dadosPima[n].bitPos = 0;
  }
  preparaStartBit();
}


void pimaSwEventLoop() {
  // rotina ciclica de verificação dos pacotes de PIMA recebidos e encaminhar para o computador com identificação
  for (int n=0;n<3;n++) {
    if (dadosPima[n].pacoteOK && !dadosPima[n].recebendo) {  // Se não está recebendo mas já tem pacote disponível
      enviaPimaSw(n);
    }
  }
}

void enviaPimaSw(uint8_t nn) {  // rotina para leitura segura do pacote do pima software serial e retransmissão para o computador
  uint8_t qtdados = dadosPima[nn].tmBuff;  // descobre quantos dados existem disponíveis
  byte tmpbuff[qtdados];  // cria um buffer temporario para ler os bytes dispiníveis
  memcpy(tmpbuff, dadosPima[nn].buff, qtdados);  // copia os dados
  dadosPima[nn].tmBuff = 0;  // "consome" o pacote
  dadosPima[nn].pacoteOK = false;  // ajusta o flag de fim de pacote
  byte tmpadd = (0x31 + nn + 3);
  int tmppos = (nn + 3);
  decode_pima_t pimatmp;
  bool pimavalido = pimaSwDecode(tmpbuff, qtdados);
  if (pimavalido && !pimaRecebido[tmppos]) {  
    // temos pima valido e ainda não recebemos nada
    pimaRecebido[tmppos] = true;  // informa que esta posição recebeu
    bcdtochar(pima[tmppos], decodePima.idmed, 5);  // aqui converte de BCD para ASCII e já deixa no buffer de retorno
  }
  if (DEBUG) {
    Serial.print("Pacote Recebido na Serial ");
    Serial.print(nn+4);
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
  } else {  // nao precisa mais enviar dados brutos ao slave...
    //Serial.write(tmpadd);
    //Serial.write(tmpbuff, qtdados);
  }
}

bool pimaSwDecode(byte *tmpbuff, int tmptam) {
  //  tmpret  - struct de retorno dos dados decodificados
  //  tmpbuff - buffer de entrada a ser decodificado
  //  tmptam  - tamanho do buffer de entrada
  //  retorno: status da decodificação (falso significa pima não encontrado, tamanho inválido ou CRC inválido)
  int buffstart = 0;  // posição onde foi encontrado o inicio do PIMA
  bool tmpvalidar = false;  // variável que indica se retornaremos um pacote pima válido
  for (int n=0;n<tmptam;n++) {
    if (tmpbuff[n] == 0xAA) {  // achamos o inicio do pacote PIMA
      // vamos iniciar uma sequencia de testes para saber se o buffer contém um pacote PIMA completo
      if ((tmptam - n) > 7) {  // Tem pelo menos 8 bytes no buffer. O oitavo byte é o numero de bytes deste pacote
        byte dadostam = tmpbuff[n+7];
        if (tmptam >= (10 + dadostam)) {  // temos todos os bytes, o pacote está completo!
          for (int m=0;m<5;m++) {
            // Pima ID
            decodePima.idmed[m] = tmpbuff[n+2+m];
          }
          // Tamanho
          decodePima.tamanho = dadostam;
          for (int m=0;m<2;m++) {
            // Escopo + Indice
            decodePima.escind[m] = tmpbuff[n+8+m];
          }
          for (int m=0;m<(dadostam-2);m++) {
            // Dados
            decodePima.dados[m] = tmpbuff[n+10+m];
          }
          for (int m=0;m<2;m++) {
            // CRC
            decodePima.crc[m] = tmpbuff[n+8+dadostam+m];
          }
          // vamos validar o CRC do pacote
          byte tmpcrccalc[6+dadostam];  // estamos retirando o cabecalho e o próprio CRC
          for (int m=0;m<(6+dadostam);m++) {
            tmpcrccalc[m] = tmpbuff[n+2+m];
            //Serial.print(tmpcrccalc[m], HEX);
          }
          //Serial.println();
          uint16_t tmpcrc = crc_16(tmpcrccalc, 6+dadostam);
          uint8_t crclo = tmpcrc;
          uint8_t crchi = tmpcrc >> 8;
          tmpvalidar = (crclo == decodePima.crc[0]) && (crchi == decodePima.crc[1]); // compara o CRC recebido e o calculado
          //Serial.print("Validacao do Pima: ");
          //Serial.println(tmpvalidar);
          //Serial.println(tmpcrc, HEX);
        }
      }
    }
  }
  return tmpvalidar;
}

ISR(TIMER3_COMPA_vect)  // interrupção por igualdade de comparação no TIMER3 Comparador A
{
  // timer de decodificação de bit do PIMA 1
  // cada vez que entramos aqui, estamos "no meio do bit" atual, conforme frequencia do timer configurado (baud)
  byte novoBit = PIND & B00000001;  // pega o valor do pino 0 no port D
  novoBit = novoBit >> 0;  // coloca o bit na posição 0
  if (dadosPima[0].bitPos < 8) {
    // estamos no meio do byte. Receber e colocar no buffer temporário
    // converter o nivel logico conforme a constante do sinal que está sendo utilizado
    if (novoBit == MARK) {
      novoBit = true;
    } else {
      novoBit = false;
    }
    novoBit = novoBit << dadosPima[0].bitPos;  // coloca na posição que deveria estar conforme bit sendo recebido (LSB > MSB)
    dadosPima[0].dado += novoBit;  // coloca o bit recebido no buffer
    // prepara recepção do próximo byte
    TCNT3 = 0x00;  // zera o contador do timer
    OCR3A = TCNT3 + TIMER_CONT;  // ajusta o valor do comparador do timer
  } else if (dadosPima[0].bitPos == 8) {
    // este é o Stop Bit! vamos validar e salvar caso OK!
    if (novoBit == MARK) {
      // vamos verificar se não estourou o tamanho do buffer de recepção antes de salvar
      if (dadosPima[0].tmBuff < RXBUF) {
        // coloca o byte recebido no buffer de recepção
        dadosPima[0].buff[dadosPima[0].tmBuff] = dadosPima[0].dado;
        dadosPima[0].tmBuff += 1;
      } else {
        // Buffer Overflow!! 
        // Colocar alguma rotina/flag aqui para tratar este caso, se necessário
        dadosPima[0].ovf = true;
      }
    } else {
      // Erro no Stop Bit!!
      // Colocar alguma rotina/flag aqui para tratar este caso, se necessário
      dadosPima[0].stpb = true;
    }
    // inicializar contador de fim de pacote
    TCNT3 = 0x00;  // zera o contador do timer
    OCR3A = TCNT3 + TIMER_PACK;  // ajusta o valor do comparador do timer para valor de fim de pacote
    // finalizar recepção
    // preparar para receber um novo byte
    EIFR  |=  B00000001;  // limpa a flag de interrupção do INT0
    EIMSK |=  B00000001;  // liga a interrupção que detecta o start bit
  } else if (dadosPima[0].bitPos == 9) {
    // se chegou aqui nesta condição, já se passou "timeout" ms desde o ultimo byte, ou seja, fim do pacote.
    // seta flag de fim de pacote e desliga a interrupção do timer
    TIMSK3 &= B11111100;  // desliga interrupção no timer 3
    dadosPima[0].recebendo = false;  // informa que está ocioso
    dadosPima[0].pacoteOK = true;  // informa que terminou de receber um pacote (devido ao silencio desde ultimo byte recebido)
  }
  dadosPima[0].bitPos += 1;  // incrementa contador de bits recebidos
}

ISR(TIMER4_COMPA_vect)          // interrupção por igualdade de comparação no TIMER4
{
  // timer de decodificação de bit do PIMA 2
  // cada vez que entramos aqui, estamos "no meio do bit" atual, conforme frequencia do timer configurado (baud)
  byte novoBit = PIND & B00000010;  // pega o valor do pino 1 no port D
  novoBit = novoBit >> 1;  // coloca o bit na posição 0
  if (dadosPima[1].bitPos < 8) {
    // estamos no meio do byte. Receber e colocar no buffer temporário
    // converter o nivel logico conforme a constante do sinal que está sendo utilizado
    if (novoBit == MARK) {
      novoBit = true;
    } else {
      novoBit = false;
    }
    novoBit = novoBit << dadosPima[1].bitPos;  // coloca na posição que deveria estar conforme bit sendo recebido (LSB > MSB)
    dadosPima[1].dado += novoBit;  // coloca o bit recebido no buffer
    // prepara recepção do próximo byte
    TCNT4 = 0x00;  // zera o contador do timer
    OCR4A = TCNT4 + TIMER_CONT;  // ajusta o valor do comparador do timer
  } else if (dadosPima[1].bitPos == 8) {
    // este é o Stop Bit! vamos validar e salvar caso OK!
    if (novoBit == MARK) {
      // vamos verificar se não estourou o tamanho do buffer de recepção antes de salvar
      if (dadosPima[1].tmBuff < RXBUF) {
        // coloca o byte recebido no buffer de recepção
        dadosPima[1].buff[dadosPima[1].tmBuff] = dadosPima[1].dado;
        dadosPima[1].tmBuff += 1;
      } else {
        // Buffer Overflow!! 
        // Colocar alguma rotina/flag aqui para tratar este caso, se necessário
        dadosPima[1].ovf = true;
      }
    } else {
      // Erro no Stop Bit!!
      // Colocar alguma rotina/flag aqui para tratar este caso, se necessário
      dadosPima[1].stpb = true;
    }
    // inicializar contador de fim de pacote
    TCNT4 = 0x00;  // zera o contador do timer
    OCR4A = TCNT4 + TIMER_PACK;  // ajusta o valor do comparador do timer para valor de fim de pacote
    // finalizar recepção
    // preparar para receber um novo byte
    EIFR  |=  B00000010;  // limpa a flag de interrupção do INT1
    EIMSK |=  B00000010;  // liga a interrupção que detecta o start bit
  } else if (dadosPima[1].bitPos == 9) {
    // se chegou aqui nesta condição, já se passou "timeout" ms desde o ultimo byte, ou seja, fim do pacote.
    // seta flag de fim de pacote e desliga a interrupção do timer
    TIMSK4 &= B11111100;  // desliga interrupção no timer 4
    dadosPima[1].recebendo = false;  // informa que está ocioso
    dadosPima[1].pacoteOK = true;  // informa que terminou de receber um pacote (devido ao silencio desde ultimo byte recebido)
  }
  dadosPima[1].bitPos += 1;  // incrementa contador de bits recebidos
}

ISR(TIMER5_COMPA_vect)          // interrupção por igualdade de comparação no TIMER5
{
  // timer de decodificação de bit do PIMA 3
  // cada vez que entramos aqui, estamos "no meio do bit" atual, conforme frequencia do timer configurado (baud)
  byte novoBit = PINE & B00010000;  // pega o valor do pino 4 no port E
  novoBit = novoBit >> 4;  // coloca o bit na posição 0
  if (dadosPima[2].bitPos < 8) {
    // estamos no meio do byte. Receber e colocar no buffer temporário
    // converter o nivel logico conforme a constante do sinal que está sendo utilizado
    if (novoBit == MARK) {
      novoBit = true;
    } else {
      novoBit = false;
    }
    novoBit = novoBit << dadosPima[2].bitPos;  // coloca na posição que deveria estar conforme bit sendo recebido (LSB > MSB)
    dadosPima[2].dado += novoBit;  // coloca o bit recebido no buffer
    // prepara recepção do próximo byte
    TCNT5 = 0x00;  // zera o contador do timer
    OCR5A = TCNT5 + TIMER_CONT;  // ajusta o valor do comparador do timer
  } else if (dadosPima[2].bitPos == 8) {
    // este é o Stop Bit! vamos validar e salvar caso OK!
    if (novoBit == MARK) {
      // vamos verificar se não estourou o tamanho do buffer de recepção antes de salvar
      if (dadosPima[2].tmBuff < RXBUF) {
        // coloca o byte recebido no buffer de recepção
        dadosPima[2].buff[dadosPima[2].tmBuff] = dadosPima[2].dado;
        dadosPima[2].tmBuff += 1;
      } else {
        // Buffer Overflow!! 
        // Colocar alguma rotina/flag aqui para tratar este caso, se necessário
        dadosPima[2].ovf = true;
      }
    } else {
      // Erro no Stop Bit!!
      // Colocar alguma rotina/flag aqui para tratar este caso, se necessário
      dadosPima[2].stpb = true;
    }
    // inicializar contador de fim de pacote
    TCNT5 = 0x00;  // zera o contador do timer
    OCR5A = TCNT5 + TIMER_PACK;  // ajusta o valor do comparador do timer para valor de fim de pacote
    // finalizar recepção
    // preparar para receber um novo byte
    EIFR  |=  B00010000;  // limpa a flag de interrupção do INT4
    EIMSK |=  B00010000;  // liga a interrupção que detecta o start bit
  } else if (dadosPima[2].bitPos == 9) {
    // se chegou aqui nesta condição, já se passou "timeout" ms desde o ultimo byte, ou seja, fim do pacote.
    // seta flag de fim de pacote e desliga a interrupção do timer
    TIMSK5 &= B11111100;  // desliga interrupção no timer 5
    dadosPima[2].recebendo = false;  // informa que está ocioso
    dadosPima[2].pacoteOK = true;  // informa que terminou de receber um pacote (devido ao silencio desde ultimo byte recebido)
  }
  dadosPima[2].bitPos += 1;  // incrementa contador de bits recebidos
}

ISR(INT0_vect) {  // Interrupção do Start Bit do PIMA 1
  // esta interrupção é chamada quando é detectado um start bit (borda de descida)
  // já detectamos o start bit, daqui pra frente é com o timer... desabilitando esta interrupção
  EIMSK &=  B11111110;  // Desliga interrupção do INT0
  // configurando o timer do primeiro bit com 1.5 bit de duração, para amostrarmos o nivel do sinal no meio do bit
  TCNT3 = 0x00;  // zera o contador do timer
  OCR3A = TCNT3 + TIMER_INIT;  // ajusta o valor do comparador do timer (para gerar a interrupção no meio do bit)
  TIMSK3 &= B11111100;  // Reset do Comparador
  TIFR3  |= B00000010;  // Limpa o bit de estouro de comparação
  TIMSK3 |= B00000010;  // Liga a interrupção por estouro de comparação
  // ajusta algumas variáveis
  dadosPima[0].recebendo = true;  // informa que está recebendo
  dadosPima[0].bitPos = 0;  // ajusta contador de bits recebidos
  dadosPima[0].dado = 0x00;  // inicializa buffer de recepção do buffer atual
}

ISR(INT1_vect) {  // Interrupção do Start Bit do PIMA 2
  // esta interrupção é chamada quando é detectado um start bit (borda de descida)
  // já detectamos o start bit, daqui pra frente é com o timer... desabilitando esta interrupção
  EIMSK &=  B11111101;  // Desliga interrupção do INT1
  // configurando o timer do primeiro bit com 1.5 bit de duração, para amostrarmos o nivel do sinal no meio do bit
  TCNT4 = 0x00;  // zera o contador do timer
  OCR4A = TCNT4 + TIMER_INIT;  // ajusta o valor do comparador do timer (para gerar a interrupção no meio do bit)
  TIMSK4 &= B11111100;  // Reset do Comparador
  TIFR4  |= B00000010;  // Limpa o bit de estouro de comparação
  TIMSK4 |= B00000010;  // Liga a interrupção por estouro de comparação
  // ajusta algumas variáveis
  dadosPima[1].recebendo = true;  // informa que está recebendo
  dadosPima[1].bitPos = 0;  // ajusta contador de bits recebidos
  dadosPima[1].dado = 0x00;  // inicializa buffer de recepção do buffer atual
}

ISR(INT4_vect) {  // Interrupção do Start Bit do PIMA 3
  // esta interrupção é chamada quando é detectado um start bit (borda de descida)
  // já detectamos o start bit, daqui pra frente é com o timer... desabilitando esta interrupção
  EIMSK &=  B11101111;  // Desliga interrupção do INT4
  // configurando o timer do primeiro bit com 1.5 bit de duração, para amostrarmos o nivel do sinal no meio do bit
  TCNT5 = 0x00;  // zera o contador do timer
  OCR5A = TCNT5 + TIMER_INIT;  // ajusta o valor do comparador do timer (para gerar a interrupção no meio do bit)
  TIMSK5 &= B11111100;  // Reset do Comparador
  TIFR5  |= B00000010;  // Limpa o bit de estouro de comparação
  TIMSK5 |= B00000010;  // Liga a interrupção por estouro de comparação
  // ajusta algumas variáveis
  dadosPima[2].recebendo = true;  // informa que está recebendo
  dadosPima[2].bitPos = 0;  // ajusta contador de bits recebidos
  dadosPima[2].dado = 0x00;  // inicializa buffer de recepção do buffer atual
}

void initTimers() {
  // inicialização de todos os timers e pinos de entrada
  pinMode(P1_PIN, INPUT_PULLUP);
  TCCR3A = B00000000;  // ajusta o timer no modo "Normal"
  TCCR3B = B00000010;  // ajusta o prescaler para 8
  pinMode(P2_PIN, INPUT_PULLUP);
  TCCR4A = B00000000;  // ajusta o timer no modo "Normal"
  TCCR4B = B00000010;  // ajusta o prescaler para 8
  pinMode(P3_PIN, INPUT_PULLUP);
  TCCR5A = B00000000;  // ajusta o timer no modo "Normal"
  TCCR5B = B00000010;  // ajusta o prescaler para 8
}

void preparaStartBit() {
  EICRA |= B00001010;  // Interrupção Externa configurado para borda de descida (INT0 e INT1)
  EICRB |= B00000010;  // Interrupção Externa configurado para borda de descida (INT4)
  EIFR  |= B00010011;  // limpa a flag de interrupção externa
  EIMSK |= B00010011;  // habilita interrupção externa
}
