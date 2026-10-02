#pragma once

class QWidget;

// Ícone da janela na barra de tarefas e no Alt+Tab (Windows).
// O Qt já usa o ícone do aplicativo, mas, numa janela sem moldura, o botão da barra de tarefas pode ficar sem
// logo ou borrado. Aqui o ícone embutido no .exe (resources/icons/app.ico, com 16 a 256 px) é entregue ao
// Windows direto, no tamanho exato pedido pela tela (também em 125%/150% de escala). Em outros sistemas não faz nada.
namespace IconeDaJanela {
void aplicar(QWidget *janela);
}
