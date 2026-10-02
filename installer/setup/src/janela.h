#pragma once

#include "pacote.h"

#include <windows.h>

#include <string>

// A janela do instalador/desinstalador: tudo desenhado à mão (GDI+) para ter um visual próprio —
// fundo escuro com brilhos animados, botões com efeito, barra de progresso e dicas — em um único .exe.
namespace Janela {

enum class Modo { Instalar, Desinstalar };

struct Configuracao {
    Modo modo = Modo::Instalar;
    Pacote::Conteudo pacote;           // modo Instalar
    std::wstring versao;               // texto da versão a instalar (ou instalada, no modo Desinstalar)
    std::wstring pasta;                // pasta de instalação sugerida (Instalar) ou a ser removida (Desinstalar)
    bool jaInstalado = false;          // Instalar: há uma versão anterior (vira "Atualizar")
    std::wstring versaoInstalada;
    bool aplicativoAberto = false;     // será fechado durante a instalação
    unsigned long long tamanhoEmMB = 0;
};

// Mostra a janela e roda até ser fechada. Devolve o código de saída do processo.
int executar(HINSTANCE instancia, const Configuracao &config);

// Salva uma imagem PNG de cada tela na pasta dada (para conferir o visual sem abrir a janela). Devolve true se salvou.
bool capturar(HINSTANCE instancia, const Configuracao &config, const std::wstring &pasta);

}  // namespace Janela
