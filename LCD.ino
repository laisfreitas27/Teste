
#include <U8g2lib.h>                 // Biblioteca do LCD Grafico 128x64

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=*/ A7  , /* data=*/ A8, /* CS=*/ A9, /* reset=*/ U8X8_PIN_NONE);

// Somente para o exemplo de Scrooling
u8g2_uint_t offset;      // current offset for the scrolling text
u8g2_uint_t width;      // pixel width of the scrolling text (must be lesser than 128 unless U8G2_16BIT is defined
const char *text = " WASION BR "; // scroll this text from right to left

typedef struct lcd_t
{
  bool atualiza;        // ativa rotina de atualização do texto no display
  bool deslizar;        // ativa a rotação do texto
  bool pulapagina;      // ativa rotação por pagina
  bool tictoc;          // para rotina de troca de scroll com troca de pagina
  String texto;         // texto enviado
  String rotatingText;  // texto em rotação, quando aplicavel
  uint8_t linha;        // linha atual (fixo na inicialização)
  uint8_t coluna;       // coluna inicial (fixo)
  uint8_t cont;         // contador de caracteres rolados na pagina
  uint8_t pulo;         // quantos caracteres desliza em cada volta
  uint8_t campo;        // quantos caracteres tem o campo
  uint32_t tempo;       // temporização de atualização (em milissegundos)
  uint32_t tm1;         // tempo exibindo a pagina
  uint32_t tm2;         // tempo correndo para a proxima pagina
};

lcd_t lcdTxtBox[7];
bool lcdRefresh = false;  // usado na atualização da tela do LCD no Loop Principa, para acelerar a atualização
bool lcdProtetorDeTela = false;  // flag que ativa o protetor de tela do LCD

#define  TEMPO_PAGINA   4000    // tempo exibindo cada pagina durante rotação de pagina no display
#define  TEMPO_ROLAR1    850    // tempo de rolagem devagar
#define  TEMPO_ROLAR2     35    // tempo de rolagem rapida, menor que 35 não parece fazer diferença (velocidade do SPI)

// Prototipo de funções com argumentos opcionais (limitação do Arduino IDE, que não gera automaticamente este tipo de prototipo)
String textRotate(String entrada, int pos=1);
void lcdPrint(String imprimir, int qt=18);
void lcdPageScroll(int boxn, int pos=1, int qt=18);
void lcdTextScroll(int boxn, int pos=1, int qt=18);



void lcdInit() {
  // inicialização do LCD
  u8g2.begin();  
  
  u8g2.setFont(/*u8g2_font_inb16_mr*/u8g2_font_helvB12_te); // set the target font to calculate the pixel width
  width = u8g2.getUTF8Width(text);    // calculate the pixel width of the text

  for (int n=0; n<7; n++) {  // inicialização do struct das caixas de texto no display
    lcdTxtBox[n].linha = (n * 9) + 8;  // linha
    lcdTxtBox[n].coluna = 35;  // onde o texto começa - desconta o cabeçalho
    lcdTxtBox[n].atualiza = false;
    lcdTxtBox[n].deslizar = false;
    lcdTxtBox[n].pulapagina = false;
    lcdTxtBox[n].tictoc = false;
    lcdTxtBox[n].tempo = tmrnow;
    lcdTxtBox[n].tm1 = tmrnow;
    lcdTxtBox[n].tm2 = tmrnow;
    lcdTxtBox[n].cont = 0;
    lcdTxtBox[n].pulo = 1;
    lcdTxtBox[n].campo = 18;
    lcdTxtBox[n].rotatingText = F(" ");
  }
  lcdTxtBox[0].coluna = 11;  // onde o texto começa na linha 1
  lcdTxtBox[0].campo = 22;  // tamanho do campo do Cabeçalho
  lcdTxtBox[0].pulo = 3;

  u8g2.setFontMode(0);    // enable transparent mode, which is faster
  lcdMontaTela();
  u8g2.setFont(u8g2_font_5x8_mr);  // Fonte PADRÃO
  lcdTxtBox[0].rotatingText = F("     WASION BR - TESTE DO PIMA - v 2.2.1      Leia o Codigo de Barras para iniciar      Aguarde os LEDs pararem de piscar para Fim do Ensaio      ");
  lcdTxtBox[0].deslizar = true;
  
  // ******************   TESTE DO DISPLAY   *****************
  //lcdTextbox(F("Teste 1"), 1);
  //lcdTextbox(F("Pima Nao Recebido!"), 2);
  //lcdTextbox(F("Tampa Trocada"), 3);
  //lcdTextbox(F("OK! (1234567890)"), 6);
  //lcdTextbox(F("Nao Parametrizado"), 5);
  //lcdTextbox(F("Erro:Tampa Trocada PIMA: 1234567890 Tampa: 0987654321 "), 4, true);
}

void lcdEventLoop() {
  // atualização ciclica do LCD
  if (lcdProtetorDeTela) {
    lcdIdle();
  } else {
    lcdTextAtu();
    for (int n=0;n<7;n++) {
      lcdTextScroll(n, lcdTxtBox[n].pulo, lcdTxtBox[n].campo);
      lcdPageScroll(n, lcdTxtBox[n].pulo, lcdTxtBox[n].campo);
    }
    if (lcdRefresh) {
      lcdRefresh = false;
      u8g2.updateDisplay();
    }
  }
}

void lcdIdle(void) {
  u8g2_uint_t x;

  if (tmrnow % 30 == 0) {  // isso aqui equivale a um "delay(30)", só que não é blocante
    u8g2.firstPage();
    do {
    
      // draw the scrolling text at current offset
      x = offset;
      u8g2.setFont(u8g2_font_helvB12_te);   // set the target font
      do {                // repeated drawing of the scrolling text...
        u8g2.drawUTF8(x, 40, text);     // draw the scolling text
        x += width;           // add the pixel width of the scrolling text
      } while( x < u8g2.getDisplayWidth() );    // draw again until the complete display is filled
      
      //u8g2.setFont(u8g2_font_inb16_mr);   // draw the current pixel width
      //u8g2.setCursor(0, 58);
      //u8g2.print(width);          // this value must be lesser than 128 unless U8G2_16BIT is set
      
    } while ( u8g2.nextPage() );
    
    offset-=1;              // scroll by one pixel
    if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width )  
      offset = 0;             // start over again
  }
}

void lcdMontaTela(void) {
  // tela inicial do programa de teste
  u8g2.drawFrame(0,0,128,64);
  //u8g2.updateDisplay();
  int atual = 8;
  //u8g2.setFont(u8g2_font_profont10_tf);
  //u8g2.setCursor(2, atual);
  //u8g2.print("  TESTE DO PIMA");
  u8g2.setFont(u8g2_font_helvR08_tf);
  atual += 9;
  u8g2.setCursor(2, atual);
  u8g2.print(F("Pos 1:"));
  atual += 9;
  u8g2.setCursor(2, atual);
  u8g2.print(F("Pos 2:"));
  atual += 9;
  u8g2.setCursor(2, atual);
  u8g2.print(F("Pos 3:"));
  atual += 9;
  u8g2.setCursor(2, atual);
  u8g2.print(F("Pos 4:"));
  atual += 9;
  u8g2.setCursor(2, atual);
  u8g2.print(F("Pos 5:"));
  atual += 9;
  u8g2.setCursor(2, atual);
  u8g2.print(F("Pos 6:"));
  lcdRefresh = true;  
}

void lcdBoxText(String texto, int boxn) {
  // atualiza um texto no campo indicado (1 a 6) - o campo desconta o cabeçalho, desenhado na tela inicial
  int atual = (boxn * 9) + 8;  // linha atual
  int col = 35;  // onde o texto começa - desconta o cabeçalho
  u8g2.setFont(u8g2_font_5x8_mr);
  u8g2.setCursor(col, atual);
  lcdPrint(texto);
  u8g2.updateDisplay();
}

void lcdTextScroll(int boxn, int pos=1, int qt=18) {
  if (lcdTxtBox[boxn].deslizar) {
    // foi solicitado deslizamento do texto
    if (tmrnow - lcdTxtBox[boxn].tempo > TEMPO_ROLAR1) {
      // se passou um tempo mínimo de atualização
      lcdTxtBox[boxn].rotatingText = textRotate(lcdTxtBox[boxn].rotatingText, pos);
      u8g2.setCursor(lcdTxtBox[boxn].coluna, lcdTxtBox[boxn].linha);
      lcdPrint(lcdTxtBox[boxn].rotatingText, qt);
      lcdRefresh = true;
      lcdTxtBox[boxn].tempo = tmrnow;
    }
  }
}

void lcdPageScroll(int boxn, int pos=1, int qt=18) {
  if (lcdTxtBox[boxn].pulapagina) {
    // foi solicitado deslizamento de pagina
    if (!lcdTxtBox[boxn].tictoc) {
      // estamos estaticos
      if (tmrnow - lcdTxtBox[boxn].tm1 > TEMPO_PAGINA) {
        // terminamos de exibir, chamar próxima pagina
        lcdTxtBox[boxn].tictoc = !lcdTxtBox[boxn].tictoc;
        lcdTxtBox[boxn].cont = 0;
      }
    } else {
      // estamos rolando a pagina
      if (tmrnow - lcdTxtBox[boxn].tm2 > TEMPO_ROLAR2) {
        if (lcdTxtBox[boxn].cont++ < qt) {
          lcdTxtBox[boxn].rotatingText = textRotate(lcdTxtBox[boxn].rotatingText, pos);
          u8g2.setCursor(lcdTxtBox[boxn].coluna, lcdTxtBox[boxn].linha);
          lcdPrint(lcdTxtBox[boxn].rotatingText, qt);
          lcdRefresh = true;
          //lcdAreaUpd(boxn, qt);  <- não está funcionando ainda!!
          lcdTxtBox[boxn].tm2 = tmrnow;
        } else {
          // rodamos toda a página, trocar para espera de exibição da pagina
          lcdTxtBox[boxn].tictoc = !lcdTxtBox[boxn].tictoc;
          lcdTxtBox[boxn].tm1 = tmrnow;
        }
      }
    }
  }
}


String textRotate(String entrada, int pos=1) {
  String tmp1;
  String tmp2;
  tmp1 = entrada.substring(pos);
  tmp2 = entrada.substring(0, pos);
  return (tmp1 + tmp2);
}

void lcdPrint(String imprimir, int qt=18) {
  String tmp;
  if (imprimir.length() > qt) {
    tmp = imprimir.substring(0, qt);
  } else {
    tmp = imprimir;
  }
  u8g2.print(tmp);
}

void lcdTextAtu(void) {
  // rotina de atualização das caixas de texto do LCD, quando solicitado
  bool tmpatu = false;  // flag temporária para saber se precisa atualizar a tela do LCD
  for (int boxn=0;boxn<7;boxn++) {
    if (lcdTxtBox[boxn].atualiza) {
      // pediu para atualizar esta caixa de texto
      lcdTxtBox[boxn].atualiza = false;  // só vem aqui uma vez por solicitação
      tmpatu = true;  // avisa que tem que atualizar o LCD com pelo menos uma mensagem
      u8g2.setCursor(lcdTxtBox[boxn].coluna, lcdTxtBox[boxn].linha);
      lcdPrint(lcdTxtBox[boxn].texto, lcdTxtBox[boxn].campo);
    }
  }
  if (tmpatu) {
    lcdRefresh = true;
  }
}

void lcdTextbox(String msg, int boxn, bool scrool=false) {
  // função para acesso externo.
  // Atualiza texto nos boxes do LCD
  lcdTxtBox[boxn].texto = msg;
  lcdTxtBox[boxn].atualiza = true;
  if (scrool) {
    lcdTxtBox[boxn].rotatingText = msg;
    lcdTxtBox[boxn].pulapagina = true;
    lcdTxtBox[boxn].atualiza = false;
  } else {
    lcdTxtBox[boxn].pulapagina = false;
  }
}
void lcdAreaUpd(int posn, int qt) {  //  ***** não está funcionando!!! *****
  int tiles = ((qt * 5) / 8) + 1;  // numero de tijolos 8x8
  int ini_x = 128 - (tiles * 8);  // posição x do primeiro tijolo, canto superior esquerdo
  int ini_y = (posn * 9) + 1;  // posição y do primeiro tijolo, canto superior esquerdo
  // altura em tijolos é 1, só vamos atualizar uma linha por vez
  //u8g2.updateDisplayArea(ini_x, ini_y, tiles, 1);  // envia o comando de atualização de área especifica
  u8g2.updateDisplayArea(0, 0, 8, 2);
}
