# Guia de Início Rápido

AI File Sorter ajuda a organizar ficheiros com um fluxo de trabalho centrado na revisão e uma aprovação automática opcional para pré-selecionar alterações na lista de revisão.

A IA sugere categorias, pastas de destino e nomes. A aplicação executa movimentos ou renomeações apenas depois de confirmar as alterações selecionadas na caixa de revisão.

## Primeira Execução Segura

Se for a sua primeira vez a usar o AI File Sorter, comece com uma pasta de teste pequena antes de apontar para um grande arquivo ou disco.

Boas pastas para a primeira execução incluem:

- uma cópia de 20-50 ficheiros de `Downloads`
- uma pequena pasta de limpeza com capturas de ecrã ou fotografias
- uma pasta temporária com alguns PDFs ou documentos

Isto mantém a primeira execução fácil de rever. Os seus ficheiros permanecem no seu computador quando usa modelos locais, e a IA apenas sugere categorias e nomes. Deixe as opções de aprovação automática desligadas na primeira execução para que nada seja movido ou renomeado até aprovar a lista de revisão.

A pesquisa de locais online opcional envia coordenadas GPS das fotografias arredondadas a três casas decimais para o serviço Nominatim do OpenStreetMap, mesmo com modelos locais. O fornecedor também pode ver o seu endereço IP e o horário do pedido. Ficheiros de fotografia, nomes de ficheiros e datas de captura não são enviados; o enriquecimento de categoria apenas por data permanece local.

## Configurar um LLM

Abra **Definições -> Selecionar LLM**. Escolha um modelo no menu suspenso **LLMs locais** ou um fornecedor em **Ligações API**.

**Qwen3.5 4B (Recommended)** é a primeira opção local e o predefinido para novas configurações. Oferece alta precisão para categorização e é recomendado para estruturas de pastas existentes. A sua seleção de modelo guardada anteriormente é preservada. Para usar o seu próprio ficheiro GGUF, escolha **LLM local personalizado (gguf)** e use **Adicionar**.

Para um modelo local integrado, expanda **Transferências**, clique em **Transferir**, e espere por **Model ready** antes de confirmar com **OK**. **Armazenamento de modelos** permite escolher a pasta de download. **Ver detalhes** revela a URL de download, o caminho local e informação técnica.

Para análise de imagens local, escolha um **Modelo visual** em **Modelos de análise de imagem** e faça o download dos ficheiros necessários. O modelo visual é selecionado separadamente do modelo de texto.

Em **Ligações API**, escolha Gemini, ChatGPT (OpenAI) ou um endpoint compatível com OpenAI personalizado, depois introduza os detalhes de ligação necessários e o nome do modelo. As APIs na nuvem requerem acesso à internet e enviam as entradas de análise para o serviço configurado. Endpoints personalizados também podem correr localmente.

Para analisar imagens através de uma ligação API compatível com OpenAI, ative **Modelo com capacidade visual** para essa ligação, depois selecione-a em **Modelos de análise de imagem → Modelo visual**. Não é necessário nenhum modelo local ou download de projector. Ativar suporte a visão pede-lhe para confirmar que o conteúdo das imagens será enviado para o servidor configurado.

## Verificar o Desempenho Local (Opcional)

Depois de fazer o download dos modelos, abra **Ficheiro -> Verificação de compatibilidade do sistema** e clique em **Executar benchmark**. Feche outras aplicações intensivas de CPU ou GPU primeiro. A verificação testa modelos locais descarregados, incluindo modelos GGUF personalizados.

Deixe **Backend** em **Automático (tentar CPU se a GPU falhar)**, ou escolha **Todos os backends disponíveis** para comparar opções de CPU e GPU suportadas. Os resultados são guardados separadamente para cada modelo e backend; uma falha num não exclui uma combinação funcional noutro.

Selecione um resultado e use **Repetir selecionados** para repetir essa combinação, **Repetir falhados** para repetir combinações com falha, ou **Tentar o backend escolhido** para testar o modelo selecionado com um backend específico do menu suspenso.

As recomendações estimam o desempenho local. A verificação não testa serviços API nem a precisão ao escolher pastas existentes, e não altera automaticamente o modelo selecionado.

## 1. Escolher uma Pasta

Use **Procurar** ou o painel **Explorador de Ficheiros** para escolher a pasta que pretende organizar.

Exemplos típicos:

- `Downloads`
- uma pasta de limpeza do ambiente de trabalho
- uma pasta num disco externo
- uma pasta sincronizada em rede ou na nuvem
- um arquivo de projeto

**Destino** controla para onde vão os ficheiros organizados. Deixe **Usar pasta analisada** ativado para organizar dentro da pasta selecionada, ou desligue-o e navegue para outro destino.

## 2. Escolher o Que a App Deve Fazer

Use as opções principais para decidir se a app deve:

- categorizar ficheiros em pastas
- analisar imagens
- analisar documentos
- oferecer sugestões de renomeação para ficheiros suportados

Se só quiser sugestões de renomeação, ative **Oferecer renomear ficheiros de imagem** ou **Sugerir renomear ficheiros de documentos**, depois ative **Renomear fotos sem mover** ou **Renomear documentos sem mover** para esse tipo de ficheiro.

**Organizar pastas** move pastas inteiras com o seu conteúdo e desativa **Pesquisar subpastas**. Para pesquisar ficheiros dentro de subpastas em vez disso, desligue **Organizar pastas** e ative **Pesquisar subpastas**.

## 3. Escolher Como Organizar

Novos perfis começam com **Usar estrutura de pastas existente**. As escolhas de modo de ordenação previamente guardadas são preservadas. Escolha um **Destino** que contenha as pastas que pretende usar, ou selecione **Criar pastas de categorias** para as gerar.

Escolha um modo de ordenação:

- **Criar pastas de categorias** cria pastas a partir dos nomes de categoria sugeridos.
- **Usar estrutura de pastas existente** escolhe pastas dentro da árvore do seu **Destino**. Ative **Sugerir novas pastas quando necessário** para permitir novos destinos quando nenhum se ajustar bem; deixe desligado para usar apenas pastas existentes.

Use **Criar estrutura de pastas** para adicionar uma árvore inicial ao seu destino. A sua **Língua da pasta** começa com a língua das suas categorias e pode ser alterada apenas para essa estrutura. Nomes de pastas não ingleses são traduzidos com o LLM selecionado. Reveja a pré-visualização traduzida antes de clicar **Criar**; a numeração e aninhamento das pastas são preservados.

Para pastas existentes, recomendamos **Qwen3.5 4B**. **Gemma 1.1 7B** e modelos acedidos através de uma API também são opções adequadas. Outros modelos locais mostram um aviso; pode manter o seu modelo ou alterá-lo em **Definições -> Selecionar LLM**.

No modo **Criar pastas de categorias**, escolha um estilo de categorização:

- **Mais refinado** para uso geral e agrupamento mais fino
- **Mais consistente** se quiser maior consistência de rótulos entre ficheiros semelhantes

Listas brancas de categorias limitam os nomes de categoria e subcategoria sugeridos no modo **Criar pastas de categorias**. Estão desativadas e ignoradas no modo **Usar estrutura de pastas existente**, que usa a sua árvore de destino em vez disso. A sua preferência de lista branca é preservada ao mudar de modo.

## 4. Iniciar a Análise

Clique **Analisar pasta**.

A app varre a pasta selecionada, recolhe a informação de que precisa e prepara uma lista de revisão.

## 5. Rever Antes de Aplicar Alterações

A caixa de revisão permite-inspecionar:

- categorias sugeridas
- subcategorias opcionais
- sugestões de renomeação para ficheiros suportados
- os caminhos finais de destino
- pré-visualizações de ficheiros onde suportado

Pode ajustar ou rejeitar sugestões antes de confirmar qualquer coisa.

Ative **Execução simulada (pré-visualização apenas, não mover ficheiros)** para pré-visualizar os movimentos e renomeações planeadas sem aplicá-las.

## 6. Aplicar as Alterações

Selecione as linhas que pretende processar e clique **Aplicar alterações**. A app cria quaisquer pastas necessárias e executa os movimentos ou renomeações selecionados.

**Pré-visualização de simulação**: Ative **Execução simulada (pré-visualização apenas, não mover ficheiros)** para pré-visualizar os movimentos e renomeações planeadas sem aplicá-las.

**Selecionar automaticamente sugestões de renomeação.** e **Selecionar automaticamente sugestões de organização** pré-selecionam linhas elegíveis. Ainda precisa de confirmar antes que essas alterações sejam aplicadas.

## 7. Reverter a Última Execução

Se aplicar alterações e depois quiser revertê-las, use **Editar -> Anular a última execução**.

O Desfazer é destinado à execução de ordenação confirmada mais recente. Usa o histórico de execuções registado pela app para mover ficheiros de volta e inverter renomeações suportadas quando possível.

Para melhores resultados, use Desfazer antes de iniciar outra grande limpeza na mesma pasta.

## 8. Aprender com as Suas Revisões

Quando aprova categorias na caixa de revisão, a app pode memorizar essas decisões locais e usá-las como pistas em execuções futuras. Isto não treina nem modifica o modelo de IA.

Os exemplos aprendidos são armazenados numa base de dados local separada, por isso limpar a cache normal de categorização não os remove. Para eliminar estes dados de aprendizagem local, use **Definições -> Repor comportamento aprendido**.

## Bom a Saber

- A app usa uma cache local para evitar reprocessar os mesmos ficheiros e para melhorar a consistência.
- A aprovação automática é opcional. Deixe-a desligada até estar confortável com as sugestões da app.
- As opções de imagem e documento podem ser expandidas separadamente se precisar de mais controlo.

## Se Algo Parece Errado

Verifique o seguinte primeiro:

- a pasta selecionada é a que pretendia
- as opções de análise relevantes estão ativadas
- o modo apenas-renomear não está a limitar o resultado de forma inesperada
- uma lista branca de categorias não está a restringir demasiado as sugestões

Para resolução adicional de problemas, abra **Ajuda -> Perguntas Frequentes**.
