#pragma once

#include <QDateTime>

// Preferências simples do programa, guardadas em QSettings (registro do Windows).
// As dos lembretes ficam em ui/GerenteDeLembretes; as da IA em services/IaService (IaConfig).
namespace Preferencias {

// --- Segurança: voltar à tela de login depois de ficar parado ---
constexpr int kBloqueiosPossiveis[] = {0, 5, 10, 15, 30, 60};  // minutos; 0 = nunca
int bloqueioEmMinutos();
void definirBloqueioEmMinutos(int minutos);

// --- Atualizações: consultar o GitHub ao abrir o programa ---
enum class Atualizacao { NaoPerguntado, Sim, Nao };
Atualizacao verificarAtualizacoes();
void definirVerificarAtualizacoes(bool verificar);
QDateTime ultimaVerificacaoDeAtualizacao();
void marcarVerificacaoDeAtualizacao();

}  // namespace Preferencias
