/*
 *           ***  ROTINAS DE TESTE DO PIMA  ***
 *    
 */

//  DEFINES




#define DLLEDBL            250  // Tempo de pisca-pisca dos LEDs, em milissegundos
#define DLHWSER            200  // Tempo aguardando para ligar a porta serial após ligar a corrente, em ms

#define ENSTOUT             10  // Timeout (em segundos) do ensaio do PIMA 

#define BUFFHORUSSZ        200  // Tamanho do bufer de envio serial de mensagens do Horus

#define MEU_NUMERO           6  // ID deste equipamento no sistema Horus


//  VARIAVEIS GLOBAIS

/*---------------------------------------------*/

byte ledStatusWrite[8];
boolean ledst = false;  // Global da variável de estado do pino no pisca pisca dos leds, para ficar invertendo...
uint32_t ledtmr = 0;  // Global do contador de tempo da rotina de pisca-pisca do LED

typedef struct led_status { // Struct com a lista de status dos leds
  boolean red;  // pra saber se liga o vermelho
  boolean green;  // pra saber se liga o verde
  boolean ledblink;  // pra saber se pisca ou não
  boolean ledatu;  // solicita atualização do status do led
  boolean ledpisca;  // tic-tac
} status_led;  // Nome da lista de status dos Leds

status_led statusled[6] = {
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false},
  {false, false, false, false, false},
};

uint32_t timeoutTeste = 0;  // variável que guarda o tempo inicial do ensaio, para rotinas de timeout
uint32_t toutHwSer = 0;  // utilizado para gerar o atraso de acionamento das portas seriais de hardware
bool iniciaSoUmaVez = false;  // flag para indicar se já foi solicitado o acionamento da porta serial
int testeCont = 0;  // contador de segundos de teste rodando
bool quandoTermina[6];  // indicador de mudança de estado na realização do teste de cada medidor
int resultadoTeste[6];  // código numero de erro do teste (0 - Aprovado, 1 - Não Recebido, 2 - Tampa Trocada, 3 - Sem Parametro)
char bffh[BUFFHORUSSZ];  // buffer dos dados para envio ao horus
int bffhtm = 0;  // tamanho dos dados no buffer para envio ao horus
bool enviarHorus = false;  // flag de dados prontos a enviar ao Horus


//  FUNCOES

void testeInit(void) {                            // rotinas de inicialização do teste 
  for (int nled=0;nled<6;nled++) {                // faz todos os leds vermelhos piscarem na inicialização
    statusled[nled].ledblink = true;
    statusled[nled].red = true;
    statusled[nled].green = false;
  }
}

void testeEventLoop(void) {                       // rotina de atualização dos eventos de teste do pima
  testeAtu();
  //ledAtu();
  enviarHorusAtu();
}

void testeAtu(void) {                             // rotina principal de teste
  if (iniciarEnsaio) {
    ligaCorrente();                               // habilita a passagem de corrente nos medidores para realizar a inspeção visual
    toutHwSer = tmrnow;                           // inicializa contador de atraso de inicialização da porta serial em relacao a ligar a corrente
    iniciaSoUmaVez = true;                        // habilita rotina que solicita ligar seriais após timeout       
    
    for (int n=0;n<6;n++) {                       // prepara todo o ambiente de teste e reinicia todas as variáveis necessárias
      quandoTermina[n] = false;                   // estado inicial, nenhum medidor deve ter terminado de receber
      resultadoTeste[n] = 1;                      // inicializa com um erro basico (que resultaria de um timeout)
      pimaRecebido[n] = false;                    // libera para receber novo pima no buffer
      ledSet(n, 8);                               // bota os LEDs para piscar Acqua (status: aguardando leitura do código de barras)
      lcdTextbox("                  ", n+1);
    }
    timeoutTeste = tmrnow;                              // reset do contador de timeout do esnaio
    testeCont = 0;                                      // contador de segundos
    lcdTextbox("__________ <--    ", 1);
    podeEnviar = true;                                  // libera entrada de codigo de barras
    iniciarEnsaio = false;                              // não volta aqui de novo sem ser solicitado novamente
    rodarEnsaio = false;                                // se isso foi um reinicio, cancelamos o ensaio anterior
    numTampa = 0;                                       // reinicia contador de recebimento de numeração da tampa
  } else if ((iniciaSoUmaVez) && (tmrnow - toutHwSer > DLHWSER)) {
      iniciaSoUmaVez = false;                           // so vem aqui uma vez a cada estouro de timer
      ligarHwSerial = true;                             // ajusta flag que indica que as seriais de hardware devem ser inicializadas
  }
  
  if (rodarEnsaio) {                                    // rotina que executa o ensaio
    podeEnviar = false;                                 // bloqueia leitura de codigo de barras (até ler "INICIAR" novamente)
    bool tmpterminar = true;                            // vai verificando todas as posições e já atualizando seu status conforme vai recebendo o pima
    if (!sempima) {  // Não verifica a rotina do PIMA se não estamos validando um medidor sem PIMA
      // Só vai finalizar lendo outro código manualmente
      for (int n=0;n<6;n++) {     
        if ((!quandoTermina[n]) && (pimaRecebido[n])) {
          quandoTermina[n] = true;                        // só vem aqui uma vez
          String tmpPima = "";                            // chegou novo Pima! Vamos verificar o resultado...
          String tmpTampa = "";
          for (int m=0; m<10; m++) {                      // vamos preparar as strings de comparação
            tmpPima += (char)pima[n][m];
            tmpTampa += (char)tampa[n][m];
          }
          if (tmpPima == tmpTampa) {                      // Medidor Aprovado!
            resultadoTeste[n] = 0;
            ledSet(n, 5);
          } 
          else if (tmpPima == "0000000000") {             // Medidor sem parâmetro!
            resultadoTeste[n] = 3;
            ledSet(n, 4);
          } 
          else {                                          // Tampa Trocada
            resultadoTeste[n] = 2;
            ledSet(n, 4);
          }
        }
        tmpterminar &= quandoTermina[n];                  // compara todas as posições. se alguma não terminou resultado é falso.
      }                                                   // vamos conferir se todos já terminaram                                 
      if (tmpterminar) {                                  // FIM DO ENSAIO! 
        rodarEnsaio = false;
        exibeResultado();
      } else if (tmrnow - timeoutTeste > 1000) {            //  passou 1 segundo...
        testeCont += 1;                                   //  incrementa contador de tempo de teste
        if (testeCont >= ENSTOUT) {                       //  TERMINOU O ENSAIO por timeout, devido a uma ou mais posições...
          rodarEnsaio = false;
          exibeResultado();
        } else {
          timeoutTeste = tmrnow;                          // reinicia contador
        }
      } else if (cancelar) {            //  foi lido um código de barras para finalizar o ensaio
        rodarEnsaio = false;
        exibeResultado();
      }
    } else {
      // Faz alguma coisa quando o ensaio é sem PIMA
      if (cancelar) {            //  foi lido um código de barras para finalizar o ensaio
        rodarEnsaio = false;
        exibeResultado();
      }
    }
  }
}

void exibeResultado(void) {                             // rotina que formata o resultado do ensaio no display e LEDs      
  desligaCorrente();                                    // desabilita a passagem de corrente nos medidores ao término do ensaio
  int cicloTotal = (int)((tmrnow - cicloInicio)/1000);  // ciclo total do ensaio, em segundos
  if (!sempima && cancelar) {
    // nesta condição específica, o cancelamento foi forçado em vez de esperar o fim do teste do PIMA
    for (int n=0;n<6;n++) {
      resultadoTeste[n] = 5;
    }
  }
  if (!sempima) {
    for (int n=0;n<6;n++) {                               // formata uma a uma a mensagem de todas as posicoes
      String tmpPima = "";
      String tmpTampa = "";
      for (int m=0;m<10;m++) {                            // vamos preparar as strings de comparação
        tmpPima += (char)pima[n][m];
        tmpTampa += (char)tampa[n][m];
      }
      String tmpmsg = "";
      if (resultadoTeste[n] == 0) {                       // caso medidor Aprovado
        ledSet(n, 2);
        tmpmsg = "OK! (" + tmpPima + ")";
        lcdTextbox(tmpmsg, n+1);
      } else if (resultadoTeste[n] == 1) {                  // caso Pima Não Recebido
        ledSet(n, 1);
        tmpmsg = "Pima Nao Recebido!";
        lcdTextbox(tmpmsg, n+1);
      } else if (resultadoTeste[n] == 2) {                  // caso Tampa Trocada
        ledSet(n, 1);
        tmpmsg = "Erro:Tampa Trocada PIMA: " + tmpPima + " Tampa: " + tmpTampa + " ";
        lcdTextbox(tmpmsg, n+1, true);
      } else if (resultadoTeste[n] == 3) {                  // Caso Sem Parametro
        ledSet(n, 1);
        tmpmsg = "Nao Parametrizado";
        lcdTextbox(tmpmsg, n+1);
      } else if (resultadoTeste[n] == 5) {                  // Caso Ensaio Cancelado
        ledSet(n, 3);
        tmpmsg = "Ensaio Cancelado!";
        lcdTextbox(tmpmsg, n+1);
      }
    }
    if (!semhorus) {
      bffhtm = 0;  // reset do ponteiro do buffer de envio
      bffh[bffhtm++] = 'H';  // no primeiro byte, coloca o código do comando na placa Slave
      if (true) {  // serve para "jogar fora" as variaveis temporarias 
        char tmpnumero[3];
        sprintf(tmpnumero, "%02d", MEU_NUMERO);  // formatar o numero da Jiga no sistema Horus
        bffh[bffhtm++] = tmpnumero[0];
        bffh[bffhtm++] = tmpnumero[1];
        char tmpciclo[4];
        sprintf(tmpciclo, "%03d", cicloTotal);  // formatar o tempo de ciclo
        bffh[bffhtm++] = tmpciclo[0];
        bffh[bffhtm++] = tmpciclo[1];
        bffh[bffhtm++] = tmpciclo[2];
      }
      for (int n=0;n<6;n++) {
        // LOG dos Resultados no Horus
        // formatar string de envio dos bytes para o Slave
        // Comando: S + med(1) + erro(2) + pima(10) + ciclo(3) + id(n>0)
        //bffh[bffhtm++] = (n+1+48);  // qual medidor  - Desnecessário, vamos enviar um stringo apenas, com dados sequenciais.
        /*
         *   Tabela de Codigos de Erro:
         *   Jiga | Horus | Descrição
         *     0  |    1  | Ok! (Aprovado)
         *     1  |    9  | Pima Não Recebido (Timeout)
         *     2  |   10  | Tampa Trocada
         *     3  |   11  | Medidor Não Parametrizado  
         *     4  |   12  | Indeterminado (ensaio rodou sem pima e com horus, aprovado ou reprovado pelo operador)
         *     5  |   13  | Teste Cancelado pelo Operador
         */
        String tabela_erro[6] = {"01", "09", "10", "11", "12", "13"};
        bffh[bffhtm++] = tabela_erro[resultadoTeste[n]][0];
        bffh[bffhtm++] = tabela_erro[resultadoTeste[n]][1];
        if (resultadoTeste[n] == 1 || resultadoTeste[n] == 5) {
          for (int m=0;m<10;m++) {                            // vamos preparar as strings de comparação
            bffh[bffhtm++] = '0';
          }
        } else {
          for (int m=0;m<10;m++) {                            // vamos preparar as strings de comparação
            bffh[bffhtm++] = (char)pima[n][m];
          }
        }
        for (int m=0; m<tampaCompletaTm[n]; m++) {
          bffh[bffhtm++] = (char)tampaCompleta[n][m];
        }
        if (n == 5) {
          // ultima posiçao
          enviarHorus = true;  // avisa para enviar ao horus na rotina especifica
        } else {
          // não é a ultima posição
          bffh[bffhtm++] = '_';
        }
      }
      //if (BLOCANTE) { delay(500); wdt_reset(); }
    }
  } else {
    // SEM PIMA
    if (!semhorus) {
      // LOG dos Resultados no Horus
      // formatar string de envio dos bytes para o Slave
      // Comando: S + med(1) + erro(2) + pima(10) + ciclo(3) + id(n>0)
      bffhtm = 0;  // reset do ponteiro do buffer de envio
      bffh[bffhtm++] = 'H';  // no primeiro byte, coloca o código do comando na placa Slave
      if (true) {  // serve para "jogar fora" as variaveis temporarias 
        char tmpnumero[3];
        sprintf(tmpnumero, "%02d", MEU_NUMERO);  // formatar o numero da Jiga no sistema Horus
        bffh[bffhtm++] = tmpnumero[0];
        bffh[bffhtm++] = tmpnumero[1];
        char tmpciclo[4];
        sprintf(tmpciclo, "%03d", cicloTotal);  // formatar o tempo de ciclo
        bffh[bffhtm++] = tmpciclo[0];
        bffh[bffhtm++] = tmpciclo[1];
        bffh[bffhtm++] = tmpciclo[2];
      }
      for (int n=0;n<6;n++) {
        // Formata uma mensagem de retorno
        String tmpmsg = "";
        ledSet(n, 3);
        tmpmsg = "Ensaio Finalizado!  Teste sem Pima   Inspecao Visual  ";
        lcdTextbox(tmpmsg, n+1, true);
        // LOG dos Resultados no Horus
        // formatar string de envio dos bytes para o Slave
        // Comando: S + med(1) + erro(2) + pima(10) + ciclo(3) + id(n>0)
        //bffh[bffhtm++] = (n+1+48);  // qual medidor  - Desnecessário, vamos enviar um stringo apenas, com dados sequenciais.
        /*
         *   Tabela de Codigos de Erro:
         *   Jiga | Horus | Descrição
         *     0  |    1  | Ok! (Aprovado)
         *     1  |    9  | Pima Não Recebido (Timeout)
         *     2  |   10  | Tampa Trocada
         *     3  |   11  | Medidor Não Parametrizado  
         *     4  |   12  | Indeterminado (ensaio rodou sem pima e com horus, aprovado ou reprovado pelo operador)
         *     5  |   13  | Teste Cancelado pelo Operador
         */
        String tabela_erro[6] = {"01", "09", "10", "11", "12", "13"};
        bffh[bffhtm++] = tabela_erro[4][0];
        bffh[bffhtm++] = tabela_erro[4][1];
        if (true) {  // quando nao tem pima no medidor, manda sempre zero apenas para preencher o protocolo de tamanho (quase) fixo
          for (int m=0;m<10;m++) {                            // vamos preparar as strings de comparação
            bffh[bffhtm++] = '0';
          }
        } else {
          for (int m=0;m<10;m++) {                            // vamos preparar as strings de comparação
            bffh[bffhtm++] = (char)pima[n][m];
          }
        }
        for (int m=0; m<tampaCompletaTm[n]; m++) {
          bffh[bffhtm++] = (char)tampaCompleta[n][m];
        }
        if (n == 5) {
          // ultima posiçao
          enviarHorus = true;  // avisa para enviar ao horus na rotina especifica
        } else {
          // não é a ultima posição
          bffh[bffhtm++] = '_';
        }
      }
    }
  }
}

void ledSet(uint8_t nled, uint8_t tmp) {                // Rotina de ajuste do Status dos LEDs
  enviaSerialLed(nled,tmp);
}

void bcdtochar(byte* retorno, byte* bcd, int tam) {       //  rotina que converte numero codificado em BCD para char array
                                                          //  ATENCAO no tamanho do array de retorno, tem que ser duas vezes maior que o tamanho do BCD
  for(int n=0;n<tam;n++) {
    byte este = bcd[n] >> 4;  
    retorno[(n*2)] = este + 0x30;
    este = bcd[n] & 0x0F;
    retorno[(n*2)+1] = este + 0x30;
  }
}

void enviaSerialLed(uint8_t n,uint8_t temp){ 
  // Rotina de Envio do código de controle dos LEDs para a placa auxiliar
  String envtmp = "L";  // Código de ajuste do LED via Serial
  envtmp += n+1;  // coloca qual a posiçao do LED
  envtmp += temp;  // coloca qual o código de cor do LED
  Serial.print(envtmp);  // envia o comando ao Slave
  Serial.print('\n');
}

void enviarHorusAtu(void) {
  // rotina que envia mensagem ao Horus que já estiver pronta
  if (enviarHorus) {
    enviarHorus = false;  // vem só uma vez por solicitação
    if (BLOCANTE) { wdt_reset(); delay(DLYBLOCK); wdt_reset(); }  // conforme o caso, precisamos que a Serial desocupe primeiro
    for (int n=0; n<bffhtm; n++) {
      Serial.print(bffh[n]);
    }
    Serial.print('\n');
  }
}
