#!/usr/bin/env python3
"""Monta o instalador do Caderno+: anexa ao CadernoSetup.exe (o "stub") os arquivos do programa comprimidos.

Uso:
    python installer/empacotar.py --stub build-setup/Release/CadernoSetup.exe \
        --pasta dist/ProfOrganizer --versao 1.1.0 --saida release/Caderno-Setup-1.1.0.exe

Formato (veja installer/setup/src/pacote.h): [stub][entradas...][metadados][rodapé de 48 bytes].
Cada arquivo é comprimido com deflate "cru" (zlib, nível 9) e leva o CRC-32 do conteúdo original.
"""
import argparse
import os
import struct
import sys
import zlib

MAGIA = b"CDRNPAY1"
PROIBIDOS = set('<>:"|?*')


def coletar(pasta, excluir):
    arquivos = []
    for raiz, pastas, nomes in os.walk(pasta):
        pastas.sort()
        for nome in sorted(nomes):
            if nome.lower() in excluir:
                continue
            caminho = os.path.join(raiz, nome)
            rel = os.path.relpath(caminho, pasta).replace(os.sep, "/")
            if any(c in PROIBIDOS or ord(c) < 32 for c in rel) or ".." in rel.split("/"):
                sys.exit("Nome de arquivo inválido para o instalador: %r" % rel)
            arquivos.append((rel, caminho))
    return arquivos


def empacotar(stub, pasta, versao, saida, exe, excluir, inseguro=False):
    with open(stub, "rb") as f:
        dados_stub = f.read()
    if dados_stub[:2] != b"MZ":
        sys.exit("O stub não parece um executável do Windows: %s" % stub)
    arquivos = coletar(pasta, excluir)
    if not any(rel.lower() == exe.lower() for rel, _ in arquivos):
        sys.exit("O programa %s não está em %s" % (exe, pasta))

    corpo = bytearray(dados_stub)
    total = 0
    guardados = 0
    lista = []
    for rel, caminho in arquivos:
        with open(caminho, "rb") as f:
            lista.append((rel, f.read()))
    if inseguro:  # SÓ PARA TESTE: um caminho que foge da pasta de destino (o instalador precisa recusar o pacote)
        lista.append(("../fora-da-pasta.txt", b"nao deveria ser gravado"))
    for rel, bruto in lista:
        crc = zlib.crc32(bruto) & 0xFFFFFFFF
        compressor = zlib.compressobj(9, zlib.DEFLATED, -15)
        comprimido = compressor.compress(bruto) + compressor.flush()
        metodo = 1
        if len(comprimido) >= len(bruto):  # não compensa comprimir (já comprimido)
            comprimido, metodo = bruto, 0
            guardados += 1
        nome = rel.encode("utf-8")
        corpo += struct.pack("<H", len(nome)) + nome
        corpo += struct.pack("<QQIB", len(bruto), len(comprimido), crc, metodo)
        corpo += comprimido
        total += len(bruto)

    meta = ("versao=%s\nexe=%s\nnome=Caderno+\n" % (versao, exe)).encode("utf-8")
    deslocamento_meta = len(corpo)
    corpo += meta
    rodape = MAGIA + struct.pack("<QQIIQI", len(dados_stub), deslocamento_meta, len(meta), len(arquivos), total, zlib.crc32(meta) & 0xFFFFFFFF)
    rodape += struct.pack("<I", zlib.crc32(rodape) & 0xFFFFFFFF)
    assert len(rodape) == 48, len(rodape)
    corpo += rodape

    os.makedirs(os.path.dirname(os.path.abspath(saida)), exist_ok=True)
    with open(saida, "wb") as f:
        f.write(corpo)
    print("%d arquivos (%d guardados sem compressão), %.1f MB -> instalador de %.1f MB: %s"
          % (len(arquivos), guardados, total / 1048576, len(corpo) / 1048576, saida))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--stub", required=True, help="CadernoSetup.exe compilado (sem pacote)")
    p.add_argument("--pasta", required=True, help="pasta do programa pronta para distribuir (com o .exe e as DLLs)")
    p.add_argument("--versao", required=True, help="texto da versão, ex.: 1.1.0")
    p.add_argument("--saida", required=True, help="instalador final")
    p.add_argument("--exe", default="ProfOrganizer.exe", help="nome do executável do programa")
    p.add_argument("--excluir", action="append", default=["autoteste.txt"], help="nome de arquivo a deixar de fora (repetível)")
    p.add_argument("--inseguro-para-teste", action="store_true", help="NÃO USE: gera um pacote malicioso para testar o instalador")
    a = p.parse_args()
    empacotar(a.stub, a.pasta, a.versao, a.saida, a.exe, {n.lower() for n in a.excluir}, a.inseguro_para_teste)


if __name__ == "__main__":
    main()
