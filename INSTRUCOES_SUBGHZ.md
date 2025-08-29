# Instruções para Continuação do Projeto SubGHz Bruce

## Objetivo

Finalizar e expandir as funcionalidades de captura prática, análise de protocolo e integração do CC1101 no Bruce. Este arquivo serve como guia para próximos passos e pode ser lido pelo GitHub Copilot para automação das tarefas.

---

## 1. Testes Práticos

- [ ] Realizar testes práticos com o CC1101 usando o menu "Practical Capture".
- [ ] Validar as funções de varredura de frequência, captura de sinal, análise de protocolo e monitoramento ao vivo.
- [ ] Registrar bugs, comportamentos inesperados ou limitações encontradas.

## 2. Expansão de Protocolos

- [ ] Adicionar suporte para novos protocolos SubGHz.
- [ ] Documentar requisitos de cada protocolo (timing, modulação, decodificação).
- [ ] Implementar callbacks e decoders específicos em `subghz_receiver.cpp` e `subghz_capture.cpp`.

## 3. Refino de Interface

- [ ] Melhorar feedback visual no menu (TFT) durante captura e análise.
- [ ] Adicionar logs detalhados para cada etapa do processo.
- [ ] Permitir exportação dos resultados de captura.

## 4. Gerenciamento de Memória

- [ ] Monitorar uso de memória durante capturas extensas.
- [ ] Otimizar pools e garbage collector se necessário.

## 5. Documentação Técnica

- [ ] Atualizar README e LAYOUT.md com instruções de uso das novas funções.
- [ ] Documentar exemplos de uso prático e fluxos de trabalho.

## 6. Automação para Copilot

- [ ] Este arquivo pode ser lido pelo Copilot para seguir instruções e executar tarefas automaticamente.
- [ ] Para cada item, Copilot pode:
  - Buscar arquivos relevantes
  - Propor implementações
  - Validar testes
  - Gerar documentação

---

## Como Utilizar

- Marque os itens concluídos.
- Adicione observações ou novos requisitos conforme o projeto evolui.
- Copilot pode seguir este roteiro para automação das próximas etapas.

---

## Histórico de Mudanças

- 29/08/2025: Criação inicial do roteiro de instruções.

---

## Observações

- Este arquivo deve ser mantido atualizado conforme o progresso do projeto.
- Use como referência para reuniões, planejamento e automação.
