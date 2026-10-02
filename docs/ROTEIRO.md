# Roteiro do Caderno+

Lista de funções desejadas (pedido do dono do projeto) e o que já existe. **"Feito"** = já está no programa;
**"Parcial"** = existe, mas falta parte do que foi pedido; **"Falta"** = ainda não existe.

## 1. Gestão diária de turmas e alunos

| Função pedida | Situação | O que existe / o que falta |
| --- | --- | --- |
| Chamada rápida (um toque) | Parcial | Chamada do dia com P/F/J/A e "marcar todos como presentes". Falta: marcar com um toque/teclado na lista inteira. |
| Registro de atestados / justificativas | Parcial | Justificativa em texto na falta. Falta: anexar o atestado (arquivo) à falta. |
| Frequência em gráficos | Feito | Relatórios > frequência por aluno; resumo do mês em grade. |
| Ficha do aluno 360° | Parcial | Notas, frequência, ocorrências (conduta, elogio, dificuldade, contato com a família) e observações, na ficha em PDF. Falta: **contatos dos responsáveis** (telefone, e-mail) e uma tela única com tudo. |
| Plano de aula integrado | Parcial | Tela Aulas (tema, objetivos, materiais, anexos) e o Assistente de IA gera planos. Falta: modelos prontos, **tag de habilidades da BNCC** e exportar o plano em PDF. |

## 2. Avaliação e correção

| Função pedida | Situação | O que existe / o que falta |
| --- | --- | --- |
| Grade de correção por rubricas | Falta | Critérios com peso e níveis, que calculam a nota. |
| Gerador e banco de questões | Parcial | O Assistente de IA gera questões com gabarito. Falta: **banco próprio** (por disciplina, assunto e dificuldade) e exportar prova com e sem gabarito. |
| Calculadora de notas com pesos | Parcial | Média ponderada por avaliação (peso e nota máxima) e nota de corte. Falta: pesos por **categoria** (provas, trabalhos, comportamento). |

## 3. Organização e produtividade

| Função pedida | Situação | O que existe / o que falta |
| --- | --- | --- |
| Agenda e calendário escolar | Feito (base) | Calendário, lembretes do Windows, exportação .ics. Falta: tipos de evento específicos (entrega de notas, conselho de classe, reunião de pais) com lembrete próprio. |
| Modo offline com sincronização | Parcial | O programa já é 100% offline. **Sincronizar entre aparelhos** exige um serviço na nuvem (conta, servidor, LGPD): é uma decisão de arquitetura a tomar antes. Hoje o caminho é Backup > salvar cópia em pasta sincronizada (OneDrive, Google Drive). |
| Exportação fácil (WhatsApp, e-mail, PDF) | Parcial | PDF de boletim, frequência e ficha. Falta: botões "Enviar por e-mail" / "Enviar por WhatsApp" que abrem o aplicativo com a mensagem pronta, sem enviar nada sozinho. |

## Ordem sugerida

1. Contatos dos responsáveis + ficha única do aluno (base para exportar e enviar).
2. Rubricas e pesos por categoria (impacto direto na rotina de correção).
3. Banco de questões e prova com/sem gabarito (reaproveita o Assistente de IA).
4. Plano de aula com modelos, habilidades da BNCC e PDF.
5. Enviar por e-mail / WhatsApp (abrindo o aplicativo, sem envio automático).
6. Sincronização entre aparelhos (só depois de decidir onde os dados ficam e como proteger).

Cada item novo que mexe no banco precisa de **nova migração** (nunca editar as antigas), teste no autoteste e atualização do
`docs/SEGURANCA.md` quando tocar em dados pessoais.
