#pragma once

#include <QString>

// Gerencia o tema claro/escuro. A escolha é salva em QSettings e reaplicada
// na próxima abertura do programa.
class ThemeManager {
public:
    enum class Tema { Claro, Escuro };

    // Lê o tema salvo (padrão: claro) e aplica no QApplication.
    static void carregarSalvo();
    static void aplicar(Tema tema);
    static Tema atual();
    static void alternar();

private:
    static QString montarFolhaDeEstilo(Tema tema);
};
