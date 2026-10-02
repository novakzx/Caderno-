#pragma once

#include <QDialog>

// "Sobre o Caderno+": logo, versão, autor, projeto e licenças dos componentes usados.
// Só mostra informações fixas do programa (core/BuildInfo.h); não lê nem grava dados do usuário.
class SobreDialog : public QDialog {
    Q_OBJECT
public:
    explicit SobreDialog(QWidget *parent = nullptr);
};
