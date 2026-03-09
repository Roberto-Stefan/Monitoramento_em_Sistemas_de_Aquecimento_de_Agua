/*
 * ==================================================================================
 * PROJETO: Monitor de Consumo Real (Versão Final - Documentada)
 * AUTOR: Roberto Leonel Stefan (Engenharia de Sistemas Embarcados)
 * DATA: 10 de Fevereiro de 2026
 * ----------------------------------------------------------------------------------
 * CÁLCULOS INTEGRADOS:
 * - Potência (W) = Vrms * Irms
 * - Energia (Wh) = Potência * (Tempo_ms / 3600000)
 * - Custo (R$) = (Wh / 1000) * Tarifa
 * - Contagem Regressiva = 60 - Tempo_Segundos
 * ==================================================================================
 */

#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h> 
#include <SPI.h>             

// Definição dos pinos do display
#define TFT_CS         10
#define TFT_DC          9
#define TFT_RST         8
#define ST77XX_DARKGREY 0x7BEF 

// Inicialização do display
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Definição dos pinos dos sensores
#define PIN_ZMPT       A2    // Sensor de tensão
#define PIN_ACS        A1    // Sensor de corrente

// Constantes de calibração
const int OFFSET_ADC = 512;        // Offset do ADC (0-1023)
const float FATOR_V  = 0.54;       // Fator de calibração da tensão
const float FATOR_I  = 0.185;      // Fator de calibração da corrente

// Constantes de custo
const float TARIFA_KWH = 0.87513;  // Tarifa por kWh em reais
const int   BANHOS_MES = 30;       // Número de banhos por mês

// Estados da máquina de estados
enum Estado {
  AGUARDANDO,  // Aguardando início do banho
  EM_BANHO,    // Banho em andamento
  RESUMO       // Exibindo resumo do banho
};
Estado estadoAtual = AGUARDANDO;

// Variáveis de tempo e energia
unsigned long tempoInicio = 0;      // Quando o banho começou
unsigned long ultimoCalculo = 0;     // Último cálculo de energia
unsigned long tempoTroca = 0;        // Tempo para troca de estado
float energia_Wh = 0;                 // Energia acumulada em Watt-hora

// Protótipo da função de resumo
void exibirResumo();

// Função para centralizar texto no display
void centralizarTexto(String texto, int y, uint8_t tamanho, uint16_t cor) {
  int larguraChar = 6 * tamanho;  // 6 pixels por caractere no tamanho 1
  int x = (128 - texto.length() * larguraChar) / 2;
  tft.setTextSize(tamanho);
  tft.setTextColor(cor, ST77XX_BLACK);
  tft.setCursor(x, y);
  tft.print(texto);
}

// Função para leitura RMS dos sensores
void realizarLeituraRMS(float &v_out, float &i_out) {
  long somaV2 = 0, somaI2 = 0;
  long n = 0;
  unsigned long start = micros();
  
  // Coleta por aproximadamente 33ms (1 período de 60Hz)
  while (micros() - start < 33333) {
    int leituraV = analogRead(PIN_ZMPT) - OFFSET_ADC;
    int leituraI = analogRead(PIN_ACS) - OFFSET_ADC;
    
    somaV2 += (long)leituraV * leituraV;
    somaI2 += (long)leituraI * leituraI;
    n++;
  }
  
  // Calcula RMS
  v_out = sqrt(somaV2 / (float)n) * FATOR_V;
  i_out = sqrt(somaI2 / (float)n) * FATOR_I;
}

// Configuração inicial
void setup() {
  Serial.begin(9600);
  Serial.println("Iniciando Monitor de Banho...");
  
  // Inicializa o display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);  // Orientação vertical
  tft.fillScreen(ST77XX_BLACK);
  
  // Tela de apresentação
  centralizarTexto("MONITOR V/I", 50, 1, ST77XX_CYAN);
  centralizarTexto("SISTEMA DE BANHO", 70, 1, ST77XX_WHITE);
  centralizarTexto("AGUARDE...", 90, 1, ST77XX_YELLOW);
  
  delay(2000);
  tft.fillScreen(ST77XX_BLACK);
  
  Serial.println("Sistema pronto!");
}

// Loop principal
void loop() {
  float Vrms, Irms;
  realizarLeituraRMS(Vrms, Irms);
  
  // Debug via Serial
  Serial.print("V: "); Serial.print(Vrms);
  Serial.print("V, I: "); Serial.print(Irms);
  Serial.print("A, Estado: "); Serial.println(estadoAtual);
    
  switch (estadoAtual) {
    
    // Estado: AGUARDANDO INÍCIO DO BANHO
    case AGUARDANDO:
      tft.setRotation(0);
      centralizarTexto("AGUARDANDO BANHO", 70, 1, ST77XX_WHITE);
      
      // Se detectar tensão > 45V, inicia o banho
      if (Vrms > 45.0) {
        energia_Wh = 0;
        tempoInicio = millis();
        ultimoCalculo = millis();
        estadoAtual = EM_BANHO;
        tft.fillScreen(ST77XX_BLACK);
        Serial.println("Banho iniciado!");
      }
      break;

    // Estado: BANHO EM ANDAMENTO
    case EM_BANHO:
      {
        tft.setRotation(0);
        
        // Calcula potência e energia
        float potenciaW = Vrms * Irms;
        unsigned long agora = millis();
        float dt_h = (agora - ultimoCalculo) / 3600000.0;  // delta tempo em horas
        energia_Wh += potenciaW * dt_h;
        ultimoCalculo = agora;

        // Tempo de banho em segundos
        unsigned long seg = (agora - tempoInicio) / 1000;
        
        // Desenha borda colorida (verde < 60s, vermelho >= 60s)
        uint16_t corBorda = (seg < 60) ? ST77XX_GREEN : ST77XX_RED;
        tft.drawRect(0, 0, 128, 160, corBorda);
        
        // Título
        centralizarTexto("BANHO EM CURSO", 12, 1, ST77XX_YELLOW);
        
        // Exibe medições
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setCursor(10, 35);
        tft.print("Tempo: "); tft.print(seg); tft.print("s");
        
        tft.setCursor(10, 55);
        tft.print("Tensao: "); tft.print(Vrms, 1); tft.print("V");
        
        tft.setCursor(10, 75);
        tft.print("Corr: "); tft.print(Irms, 2); tft.print("A");
        
        tft.setCursor(10, 95);
        tft.print("Pot: "); tft.print(potenciaW, 0); tft.print("W");
        
        tft.setCursor(10, 115);
        tft.print("Cons: "); tft.print(energia_Wh, 2); tft.print("Wh");

        // Alerta de tempo
        if (seg < 60) {
          int falta = 60 - seg;
          String msg = "Alerta em: " + String(falta) + "s";
          centralizarTexto(msg, 140, 1, ST77XX_GREEN);
        } else {
          centralizarTexto(">>> ALTO CONSUMO <<<", 140, 1, ST77XX_RED);
        }

        // Se tensão cair, finaliza o banho
        if (Vrms < 30.0) {
          tft.fillScreen(ST77XX_BLACK);
          exibirResumo();
          tempoTroca = millis();
          estadoAtual = RESUMO;
          Serial.println("Banho finalizado!");
        }
      }
      break;

    // Estado: EXIBINDO RESUMO DO BANHO
    case RESUMO:
      tft.setRotation(0);
      
      // Se detectar novo banho, volta para EM_BANHO
      if (Vrms > 45.0) {
        estadoAtual = EM_BANHO;
        ultimoCalculo = millis();
        tft.fillScreen(ST77XX_BLACK);
        Serial.println("Novo banho iniciado!");
      }
      
      // Após 10 segundos, volta para AGUARDANDO
      if (millis() - tempoTroca > 10000) {
        estadoAtual = AGUARDANDO;
        tft.fillScreen(ST77XX_BLACK);
        Serial.println("Voltando ao modo aguardando...");
      }
      break;
  }
  
  delay(200);  // Pequeno delay para estabilidade
}

// Função para exibir o resumo do banho
void exibirResumo() {
  float kwh = energia_Wh / 1000.0;
  float custo = kwh * TARIFA_KWH;
  float custoMensal = custo * BANHOS_MES;

  // Moldura
  tft.drawRect(0, 0, 128, 160, ST77XX_CYAN);
  
  // Título
  centralizarTexto("=== RESUMO ===", 10, 1, ST77XX_CYAN);
  
  // Consumo
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(10, 35);
  tft.print("Consumo:");
  tft.setCursor(10, 50);
  tft.print(kwh, 4);
  tft.print(" kWh");
  
  // Custo do banho
  tft.setCursor(10, 70);
  tft.print("Custo:");
  tft.setCursor(10, 85);
  tft.print("R$ ");
  tft.print(custo, 2);
  
  // Custo mensal estimado
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(10, 105);
  tft.print("Mensal:");
  tft.setCursor(10, 120);
  tft.print("R$ ");
  tft.print(custoMensal, 2);
  
  // Timer para voltar
  centralizarTexto("Voltando em 10s", 145, 1, ST77XX_WHITE);
  
  // Debug Serial
  Serial.print("Resumo - Consumo: "); Serial.print(kwh, 4);
  Serial.print(" kWh, Custo: R$ "); Serial.println(custo, 2);
}