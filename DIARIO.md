# DIÁRIO E UTILITÁRIOS DO PROJETO
- Tema: Verificador de Integridade de Disco (tema 14)
- Autoras: Alícia e Isabella
- Disciplina: Trabalho Interdisciplinar de Estrutura de Dados e Sistemas Operacionais (IFES)
- Este documento registra a evolução do projeto, os principais obstáculos técnicos enfrentados e as soluções arquiteturais adotadas com o auxílio de inteligência artificial (Gemini) ao longo do desenvolvimento.

## Comandos Mais Usados em git e GitHub
### Iniciando um projeto
- Criar repositório no site do GitHub
- git init
- git add [nome do arquivo] *
- git commit -m "[mensagem]" *
- git branch -M [nome da branch]
- git remote add origin [link do repositório]
- git push -u origin [nome da branch] -> só a primeira vez *

### Comandos adicionais
- git push: envia as alterações locais para o repositório
- git status: verifica se houve alterações
- git remote -v: verifica a url do repositório
- git remote set-url origin [nova url]: muda a url do repositório

## Criação de Imagem de Disco (.img)
- fsutil file createnew arquivo.img [tamanho em bytes]
- Cada bloco possui 4KB (~4096 bytes)

## 📓 Contribuições e Uso do Gemini Pro

### 📅 Segunda-feira: Estrutura, Regras de Negócio e Divisão

- Dificuldade / Dúvida Inicial: Como construir uma Árvore de Merkle binária quando o arquivo de imagem possui um número ímpar de blocos (ex: 3 blocos)?

- A Solução: A IA nos guiou na regra padrão de criptografia: não se duplica o bloco físico no disco, mas sim o hash do bloco isolado temporariamente na memória RAM para emparelhar consigo mesmo (Hash 2 concatenado com Hash 2).

- Organização: Estruturamos o README.md e um plano de ataque definindo escopos independentes para ganhar tempo: Alícia focada na infraestrutura de Sistemas Operacionais (I/O, leitura de disco) e Isabella na Estrutura de Dados (Criptografia e nós da Árvore).

### 📅 Terça-feira: Os Desafios do C (Ponteiros, Memória e Arquivos Binários)

- Dificuldade / Dúvida: O módulo de leitura do disco estava gerando erros críticos de compilação (invalid initializer) e retornando endereços de memória proibidos (Segmentation Fault).

- A Solução: O Gemini nos guiou por uma bateria de conceitos fundamentais do C, sem entregar o código pronto, nos forçando a construir o raciocínio:

- Entendemos a diferença entre abrir arquivos como texto ("r" com fgets) e como binários brutos ("rb" com fread).

- Corrigimos a confusão entre arrays estáticos (char []) e ponteiros dinâmicos (char *).

- Dominamos a matemática do fread(buffer, tamanho_item, qtd_itens, arquivo) para evitar ler o arquivo inteiro de uma vez ou estourar a memória (Buffer Overflow).

- Segurança de Memória: Aprendemos a importância de usar malloc para garantir o tamanho exato da caixa de leitura e a obrigatoriedade do free no final do fluxo para evitar Memory Leaks (e garantir aprovação limpa no Valgrind).

### 📅 Quarta-feira (Hoje): A Guerra dos Compiladores e a Árvore Final

- Dificuldade / Dúvida: Ao integrar a biblioteca OpenSSL, o projeto parou de compilar com um erro fatal no linker (cannot find -lcrypto e -lssl). Além disso, não sabíamos ao certo qual função usar para gerar o hash final.

- A Solução: * Criptografia Moderna: Sob a orientação da IA, abandonamos a função obsoleta SHA256() e implementamos a família moderna EVP (EVP_DigestInit_ex, Update, Final_ex), que é o padrão atual da indústria e protege contra vazamentos no contexto da OpenSSL.

- Choque de Arquiteturas: O erro do linker não era no código, mas no Sistema Operacional. Estávamos tentando compilar uma biblioteca de 64-bits com um GCC clássico de 32-bits do Windows.

- Refatoração de Ambiente: Seguimos o passo a passo para limpar as variáveis de ambiente (Path), desinstalar o MinGW antigo e instalar o toolchain moderno de 64-bits via MSYS2 (pacman).

- O Pulo do Gato (Integração): Com o ambiente saneado, a IA nos ajudou a estruturar a lógica da função genérica construir_arvore_merkle. Isso nos salvou de dois bugs catastróficos finais:

- Descobrimos que não podíamos retornar um array local da função de hash (precisava ser alocado dinamicamente com malloc para não virar lixo de memória na struct).

- Identificamos que a função de hash não podia ter o tamanho 4096 engessado (hardcoded), pois ao subir para os nós pais da árvore, estaríamos fazendo o hash de 64 bytes (concatenação de dois filhos de 32 bytes), e não mais de um bloco inteiro do disco.