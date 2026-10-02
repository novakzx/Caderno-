#pragma once

#include <QString>

// Guarda segredos (como a chave de API da IA) protegidos pelo Windows: DPAPI (CryptProtectData).
// O texto protegido só pode ser aberto pelo MESMO usuário do Windows, no MESMO computador; copiar o registro
// ou o arquivo de configurações para outro lugar não revela a chave. Não é criptografia contra malware que
// roda como o próprio usuário (nada em um programa de desktop consegue isso), mas evita a chave em texto puro.
//
// Fora do Windows não há proteção disponível: `disponivel()` é falso e o programa não grava a chave em disco.
namespace SegredoService {

bool disponivel();

// Texto -> texto protegido em base64 ("" se falhar ou se não for Windows).
QString proteger(const QString &texto);
// Texto protegido em base64 -> texto original ("" se inválido, adulterado ou de outro usuário).
QString revelar(const QString &protegido);

}  // namespace SegredoService
