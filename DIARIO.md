# DIÁRIO E UTILITÁRIOS DO PROJETO
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
